// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include "base/timer/timer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {

using password_manager::PasswordForm;
using password_manager::PasswordFormCache;
using password_manager::PasswordFormManager;
using PasswordChangeOutcome = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using ProtoTreeUpdate = optimization_guide::proto::AXTreeUpdate;

// Max numbers of nodes for the AX Tree Update Snapshot.
constexpr int kMaxNodesInAXTreeSnapshot = 5000;

PasswordFormCache& GetFormCache(content::WebContents* web_contents) {
  auto* client = static_cast<password_manager::PasswordManagerClient*>(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  CHECK(client);
  CHECK(client->GetPasswordManager());

  auto* cache = client->GetPasswordManager()->GetPasswordFormCache();
  CHECK(cache);
  return *cache;
}

// Helper object which waits for change password parsing, invokes callback on
// completion. If form isn't found withing
// `PasswordChangeDelegateImpl::kChangePasswordFormWaitingTimeout` callback is
// invoked with null.
class ParsedPasswordFormWaiter
    : public password_manager::PasswordFormManagerObserver {
 public:
  using PasswordFormFoundCallback =
      base::OnceCallback<void(password_manager::PasswordFormManager*)>;

  ParsedPasswordFormWaiter(content::WebContents* web_contents,
                           PasswordFormFoundCallback callback)
      : web_contents_(web_contents->GetWeakPtr()),
        callback_(std::move(callback)) {
    GetFormCache(web_contents).SetObserver(weak_ptr_factory_.GetWeakPtr());

    timeout_timer_.Start(
        FROM_HERE,
        PasswordChangeDelegateImpl::kChangePasswordFormWaitingTimeout, this,
        &ParsedPasswordFormWaiter::OnTimeout);
  }

  ~ParsedPasswordFormWaiter() override {
    if (web_contents_) {
      GetFormCache(web_contents_.get()).ResetObserver();
    }
  }

 private:
  // password_manager::PasswordFormManagerObserver Impl
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override {
    CHECK(callback_);
    CHECK(form_manager);

    const PasswordForm* parsed_form = form_manager->GetParsedObservedForm();
    CHECK(parsed_form);

    // New password field and password confirmation fields are indicators of
    // a change password form.
    if (!parsed_form->new_password_element_renderer_id ||
        !parsed_form->confirmation_password_element_renderer_id) {
      return;
    }

    // Do not invoke anything after calling the `callback_` as object might be
    // destroyed immediately after.
    std::move(callback_).Run(form_manager);
  }

  void OnTimeout() {
    CHECK(callback_);
    std::move(callback_).Run(nullptr);
  }

  base::OneShotTimer timeout_timer_;
  base::WeakPtr<content::WebContents> web_contents_;
  PasswordFormFoundCallback callback_;

  base::WeakPtrFactory<ParsedPasswordFormWaiter> weak_ptr_factory_{this};
};

std::u16string GeneratePassword(
    const PasswordForm& form,
    password_manager::PasswordGenerationFrameHelper* generation_helper) {
  auto iter = base::ranges::find(form.form_data.fields(),
                                 form.new_password_element_renderer_id,
                                 &autofill::FormFieldData::renderer_id);
  CHECK(iter != form.form_data.fields().end());

  return generation_helper->GeneratePassword(
      form.url,
      autofill::password_generation::PasswordGenerationType::kAutomatic,
      autofill::CalculateFormSignature(form.form_data),
      autofill::CalculateFieldSignatureForField(*iter), iter->max_length());
}

bool IsActive(base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return false;
  }
#if !BUILDFLAG(IS_ANDROID)
  // Can be null in unit tests.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents.get());
  return tab ? tab->IsActivated() : false;
#else
  return false;
#endif
}

void DisplayChangePasswordBubbleAutomatically(
    base::WeakPtr<content::WebContents> original_tab,
    base::WeakPtr<content::WebContents> tab_with_password_change) {
  content::WebContents* contents = IsActive(original_tab) ? original_tab.get()
                                   : IsActive(tab_with_password_change)
                                       ? tab_with_password_change.get()
                                       : nullptr;
  if (contents) {
    ManagePasswordsUIController::FromWebContents(contents)
        ->ShowChangePasswordBubble();
  }
}

}  // namespace

PasswordChangeDelegateImpl::PasswordChangeDelegateImpl(
    GURL change_password_url,
    std::u16string username,
    std::u16string password,
    content::WebContents* originator,
    OpenPasswordChangeTabCallback callback)
    : change_password_url_(std::move(change_password_url)),
      username_(std::move(username)),
      original_password_(std::move(password)),
      originator_(originator->GetWeakPtr()),
      open_password_change_tab_callback_(std::move(callback)) {}

PasswordChangeDelegateImpl::~PasswordChangeDelegateImpl() = default;

void PasswordChangeDelegateImpl::Init() {
  if (IsPrivacyNoticeAcknowledged()) {
    StartPasswordChange();
    return;
  }
  UpdateState(State::kWaitingForAgreement);
}

void PasswordChangeDelegateImpl::StartPasswordChange() {
  CHECK(originator_);
  content::WebContents* new_tab =
      std::move(open_password_change_tab_callback_)
          .Run(change_password_url_, originator_.get());
  if (new_tab) {
    executor_ = new_tab->GetWeakPtr();
    form_waiter_ = std::make_unique<ParsedPasswordFormWaiter>(
        new_tab,
        base::BindOnce(&PasswordChangeDelegateImpl::OnPasswordChangeFormParsed,
                       weak_ptr_factory_.GetWeakPtr()));

    Observe(new_tab);
  }
}

void PasswordChangeDelegateImpl::OnPasswordChangeFormParsed(
    password_manager::PasswordFormManager* form_manager) {
  form_waiter_.reset();

  if (!form_manager) {
    UpdateState(State::kChangePasswordFormNotFound);
    return;
  }

  form_manager_ = form_manager->Clone();

  // Post task is required because when PasswordFormManager parses a form
  // SendFillInformationToRenderer is invoked after OnPasswordFormParsed,
  // potentially clearing agent state and preventing successful login detection.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordChangeDelegateImpl::FillChangePasswordForm,
                     weak_ptr_factory_.GetWeakPtr(),
                     *form_manager->GetParsedObservedForm(),
                     form_manager->GetDriver()));
}

void PasswordChangeDelegateImpl::WebContentsDestroyed() {
  // PasswordFormManager keeps raw pointers to PasswordManagerClient reset it
  // immediately to avoid keeping dangling pointer.
  form_manager_.reset();
}

bool PasswordChangeDelegateImpl::IsPasswordChangeOngoing(
    content::WebContents* web_contents) {
  return (originator_ && originator_.get() == web_contents) ||
         (executor_ && executor_.get() == web_contents);
}

PasswordChangeDelegate::State PasswordChangeDelegateImpl::GetCurrentState()
    const {
  return current_state_;
}

void PasswordChangeDelegateImpl::Stop() {
  observers_.Notify(&PasswordChangeDelegate::Observer::OnPasswordChangeStopped,
                    this);
}

void PasswordChangeDelegateImpl::OnPasswordFormSubmission(
    content::WebContents* web_contents) {
  if (executor_ && executor_.get() == web_contents && form_manager_ &&
      !submission_detected_) {
    submission_detected_ = true;
    web_contents->RequestAXTreeSnapshot(
        base::BindOnce(&PasswordChangeDelegateImpl::ProcessTree,
                       weak_ptr_factory_.GetWeakPtr()),
        ui::AXMode::kWebContents, kMaxNodesInAXTreeSnapshot,
        /* timeout= */ {}, content::WebContents::AXTreeSnapshotPolicy::kAll);
  }
}

void PasswordChangeDelegateImpl::ProcessTree(ui::AXTreeUpdate& ax_tree_update) {
  ProtoTreeUpdate ax_tree_proto;
  optimization_guide::PopulateAXTreeUpdateProto(ax_tree_update, &ax_tree_proto);
  // Construct request.
  optimization_guide::proto::PasswordChangeRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_proto);

  OptimizationGuideKeyedService* optimization_executor =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(executor_->GetBrowserContext()));
  optimization_executor->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kPasswordChangeSubmission,
      request,
      /*execution_timeout=*/std::nullopt,
      base::BindOnce(&PasswordChangeDelegateImpl::OnExecutionResponseCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeDelegateImpl::OnExecutionResponseCallback(
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!execution_result.response.has_value()) {
    UpdateState(State::kPasswordChangeFailed);
    return;
  }
  std::optional<optimization_guide::proto::PasswordChangeResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::PasswordChangeResponse>(
          execution_result.response.value());
  if (!response) {
    UpdateState(State::kPasswordChangeFailed);
    return;
  }
  PasswordChangeOutcome outcome =
      response.value().outcome_data().submission_outcome();
  if (outcome ==
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME ||
      outcome ==
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_UNKNOWN_OUTCOME) {
    SuccessfulSubmissionDetected();
  } else {
    UpdateState(State::kPasswordChangeFailed);
  }
}

#if !BUILDFLAG(IS_ANDROID)
void PasswordChangeDelegateImpl::OpenPasswordChangeTab() {
  if (executor_) {
    auto* tab_interface = tabs::TabInterface::GetFromContents(executor_.get());
    CHECK(tab_interface);

    auto* tabs_strip =
        tab_interface->GetBrowserWindowInterface()->GetTabStripModel();
    tabs_strip->ActivateTabAt(
        tabs_strip->GetIndexOfWebContents(executor_.get()));
  }
}
#endif

void PasswordChangeDelegateImpl::SuccessfulSubmissionDetected() {
  if (form_manager_) {
    form_manager_->OnUpdateUsernameFromPrompt(username_);
    form_manager_->Save();
    UpdateState(State::kPasswordSuccessfullyChanged);
  }
}

void PasswordChangeDelegateImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordChangeDelegateImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::u16string PasswordChangeDelegateImpl::GetDisplayOrigin() const {
  GURL url = form_manager_ ? form_manager_->GetURL() : change_password_url_;
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

const std::u16string& PasswordChangeDelegateImpl::GetUsername() const {
  return username_;
}

const std::u16string& PasswordChangeDelegateImpl::GetGeneratedPassword() const {
  return generated_password_;
}

void PasswordChangeDelegateImpl::OnPrivacyNoticeAccepted() {
  Profile* profile =
      Profile::FromBrowserContext(originator_->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement, true);
  UpdateState(PasswordChangeDelegate::State::kWaitingForChangePasswordForm);
  StartPasswordChange();
}

void PasswordChangeDelegateImpl::UpdateState(
    PasswordChangeDelegate::State new_state) {
  if (new_state != current_state_) {
    current_state_ = new_state;
    observers_.Notify(&PasswordChangeDelegate::Observer::OnStateChanged,
                      current_state_);

    switch (current_state_) {
      case State::kWaitingForChangePasswordForm:
      case State::kChangingPassword:
        return;
      case State::kWaitingForAgreement:
      case State::kChangePasswordFormNotFound:
      case State::kPasswordSuccessfullyChanged:
      case State::kPasswordChangeFailed:
        DisplayChangePasswordBubbleAutomatically(originator_, executor_);
        break;
    }
  }
}

void PasswordChangeDelegateImpl::ChangePasswordFormFilled(
    const autofill::FormData& submitted_form) {
  form_manager_->ProvisionallySave(
      submitted_form, form_manager_->GetDriver().get(),
      base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                     password_manager::PossibleUsernameData>(
          password_manager::kMaxSingleUsernameFieldsToStore));
}

void PasswordChangeDelegateImpl::FillChangePasswordForm(
    password_manager::PasswordForm form,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver) {
  if (!form_manager_ || !driver || !driver->GetPasswordGenerationHelper()) {
    return;
  }

  generated_password_ =
      GeneratePassword(form, driver->GetPasswordGenerationHelper());

  driver->SubmitChangePasswordForm(
      form.password_element_renderer_id, form.new_password_element_renderer_id,
      form.confirmation_password_element_renderer_id, original_password_,
      generated_password_,
      base::BindOnce(&PasswordChangeDelegateImpl::ChangePasswordFormFilled,
                     weak_ptr_factory_.GetWeakPtr()));

  form_manager_->PresaveGeneratedPassword(form.form_data, generated_password_);
  UpdateState(PasswordChangeDelegate::State::kChangingPassword);
}

bool PasswordChangeDelegateImpl::IsPrivacyNoticeAcknowledged() const {
  Profile* profile =
      Profile::FromBrowserContext(originator_->GetBrowserContext());
  return profile->GetPrefs()->GetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement);
}

base::WeakPtr<PasswordChangeDelegate> PasswordChangeDelegateImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
