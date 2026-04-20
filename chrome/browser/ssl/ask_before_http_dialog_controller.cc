// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/class_property.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/bulleted_label_list/bulleted_label_list_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#else
#include "ui/android/modal_dialog_wrapper.h"
#include "ui/android/window_android.h"
#endif

using HttpWarningReason =
    security_interstitials::https_only_mode::InterstitialReason;

namespace {

inline constexpr char kLearnMoreLink[] =
    "https://support.google.com/chrome?p=first_mode";

// Helper to create the prompt body view based on the Ask-before-HTTP warning
// type.
void AddAskBeforeHttpDialogText(ui::DialogModel::Builder& dialog_model,
                                HttpWarningReason warning_reason,
                                ui::DialogModelLabel::TextReplacement link) {
  // Build the dialog text view based on the warning type.
  if (warning_reason == HttpWarningReason::kSiteEngagementHeuristic) {
    auto description_text = ui::DialogModelLabel::CreateWithReplacement(
        IDS_ABH_PROMPT_SITE_ENGAGEMENT_PRIMARY_PARAGRAPH, link);
    dialog_model.AddParagraph(
        description_text, /*header=*/u"",
        AskBeforeHttpDialogController::kDescriptionTextId);
    return;
  } else if (warning_reason == HttpWarningReason::kAdvancedProtection) {
    // TODO(crbug.com/351990829): Android text is slightly different.
    auto description_text = ui::DialogModelLabel::CreateWithReplacement(
        IDS_ABH_PROMPT_ADVANCED_PROTECTION_PRIMARY_PARAGRAPH, link);
    dialog_model.AddParagraph(
        description_text, /*header=*/u"",
        AskBeforeHttpDialogController::kDescriptionTextId);
    return;
  } else if (warning_reason ==
             HttpWarningReason::kTypicallySecureUserHeuristic) {
    auto description_text = ui::DialogModelLabel::CreateWithReplacement(
        IDS_ABH_PROMPT_TYPICALLY_SECURE_BROWSING_PRIMARY_PARAGRAPH, link);
    dialog_model.AddParagraph(
        description_text, /*header=*/u"",
        AskBeforeHttpDialogController::kDescriptionTextId);
    return;
  } else if (warning_reason == HttpWarningReason::kIncognito) {
    auto description_text = ui::DialogModelLabel::CreateWithReplacement(
        IDS_ABH_PROMPT_INCOGNITO_PRIMARY_PARAGRAPH, link);
    dialog_model.AddParagraph(
        description_text, /*header=*/u"",
        AskBeforeHttpDialogController::kDescriptionTextId);
    return;
  } else if (warning_reason == HttpWarningReason::kPref ||
             warning_reason == HttpWarningReason::kBalanced) {
#if !BUILDFLAG(IS_ANDROID)
    // Default text includes parts as a bulleted list.
    // TODO(crbug.com/351990829): Replace this with a custom implementation.
    // The existing BulletedLabelListView is pretty minimal and doesn't allow
    // much flexibility. Having our own would allow more control over margins,
    // line heights, text context/styling, and may make it easier to bold
    // certain parts (i.e., uses StyledLabel and exposes them for
    // customization).
    auto bullet_list_view = std::make_unique<views::BulletedLabelListView>(
        std::vector<std::u16string>(
            {l10n_util::GetStringUTF16(
                 IDS_ABH_PROMPT_BALANCED_MODE_FIRST_ITEM_TEXT),
             l10n_util::GetStringUTF16(
                 IDS_ABH_PROMPT_BALANCED_MODE_SECOND_ITEM_TEXT)}),
        views::style::TextStyle::STYLE_BODY_4);

    dialog_model.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            std::move(bullet_list_view),
            views::BubbleDialogModelHost::FieldType::kText));
#else
    // Android does not support views::BulletedLabelListView, so we add the
    // list items as paragraphs.
    dialog_model.AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
        IDS_ABH_PROMPT_BALANCED_MODE_FIRST_ITEM_TEXT)));
    dialog_model.AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
        IDS_ABH_PROMPT_BALANCED_MODE_SECOND_ITEM_TEXT)));
#endif

    auto description_text = ui::DialogModelLabel::CreateWithReplacement(
        IDS_ABH_PROMPT_SECONDARY_TEXT, link);
    dialog_model.AddParagraph(
        description_text, /*header=*/u"",
        AskBeforeHttpDialogController::kDescriptionTextId);
    return;
  }
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AskBeforeHttpDialogController,
                                      kGoBackButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AskBeforeHttpDialogController,
                                      kContinueButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AskBeforeHttpDialogController,
                                      kDescriptionTextId);

DEFINE_USER_DATA(AskBeforeHttpDialogController);

// static
AskBeforeHttpDialogController* AskBeforeHttpDialogController::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

AskBeforeHttpDialogController::AskBeforeHttpDialogController(
    tabs::TabInterface* tab)
    : content::WebContentsObserver(tab->GetContents()),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {
  // Configure the metrics helper, shared across instances of the prompt for
  // this controller. This avoids cases where the metrics helper could be reset
  // while another call was trying to use it to record a user decision (such as
  // multiple concurrent calls to CloseDialogWidget()). See crbug.com/440547265.
  security_interstitials::MetricsHelper::ReportDetails settings;
  settings.metric_prefix = "https_first_mode";
  // TODO(crbug.com/351990829): Consider if we want to record repeated
  // visit metrics (for both the new dialog UI and for the old interstitial UI).
  metrics_helper_ = std::make_unique<security_interstitials::MetricsHelper>(
      GURL(), settings, nullptr);
}

AskBeforeHttpDialogController::~AskBeforeHttpDialogController() {
  CloseDialog();
}

void AskBeforeHttpDialogController::ShowDialog(
    content::WebContents* web_contents,
    const GURL& request_url,
    ukm::SourceId navigation_source_id) {
  // Track the source ID for the navigation that triggered the dialog.
  navigation_source_id_ = navigation_source_id;
  request_url_ = request_url;

  if (!is_suspended_) {
    metrics_helper_->RecordUserDecision(
        security_interstitials::MetricsHelper::SHOW);
    metrics_helper_->RecordUserInteraction(
        security_interstitials::MetricsHelper::TOTAL_VISITS);
  }
  is_suspended_ = false;

  std::unique_ptr<ui::DialogModel> dialog_model =
      CreateDialogModel(request_url);

#if !BUILDFLAG(IS_ANDROID)
  // The widget will own `model_host` through DialogDelegate.
  views::BubbleDialogModelHost* model_host =
      views::BubbleDialogModelHost::CreateModal(std::move(dialog_model),
                                                ui::mojom::ModalType::kChild)
          .release();
  model_host->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  model_host->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));

  auto tab_dialog_params = std::make_unique<tabs::TabDialogManager::Params>();
  // NOTE: This param *only* applies to cross-site navigations, and interacts
  // poorly with the ordering of when the ABH dialog is triggered (during
  // navigation completion). If this is set to true, the dialog gets shown
  // and then immediately closed when doing a cross-site navigation. We
  // instead want to observe *all* navigations and close the dialog ourselves
  // if it was present when a new navigation *starts*, which is handled by
  // HttpsOnlyModeTabHelper::DidStartNavigation().
  tab_dialog_params->close_on_navigate = false;
  // If for whatever reason a new ABH dialog is triggered, we should prefer
  // showing that as it is the one that is relevant for the _current_
  // navigation. This will cause any existing dialog to be dismissed.
  // TODO(crbug.com/351990829): Write a test for the current behavior that the
  // dialog being dismissed for this reason doesn't trigger a "back to
  // safety" action.
  tab_dialog_params->block_new_modal = false;

  auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  CHECK(tab);
  dialog_widget_ =
      tab->GetTabFeatures()->tab_dialog_manager()->CreateAndShowDialog(
          model_host, std::move(tab_dialog_params));
  dialog_widget_->MakeCloseSynchronous(
      base::BindOnce(&AskBeforeHttpDialogController::CloseDialogWidget,
                     weak_ptr_factory_.GetWeakPtr()));

  // Explicitly call RequestFocus() to ensure initially focused view is focused.
  views::View* focused_view = model_host->GetInitiallyFocusedView();
  CHECK(focused_view);
  focused_view->RequestFocus();
#else
  current_dialog_model_ = dialog_model.get();
  ui::WindowAndroid* window = web_contents->GetTopLevelNativeWindow();
  ui::ModalDialogWrapper::ShowTabModal(std::move(dialog_model), window);
#endif
}

bool AskBeforeHttpDialogController::HasOpenDialog() const {
#if !BUILDFLAG(IS_ANDROID)
  return dialog_widget_ && !dialog_widget_->IsClosed();
#else
  return current_dialog_model_ != nullptr;
#endif
}

void AskBeforeHttpDialogController::CloseDialog() {
// Dialog closing events are dispatched slightly differently in Views vs. in
// Android ModalDialogWrapper. On Desktop, we just defer to CloseDialogWidget()
// to reuse code across different cases where the dialog widget is closed. On
// Android, we handle this here directly, as we can't use Views code.
#if !BUILDFLAG(IS_ANDROID)
  if (HasOpenDialog()) {
    CloseDialogWidget(views::Widget::ClosedReason::kUnspecified);
  }
#else
  if (HasOpenDialog()) {
    // Programmatically close the dialog. This will trigger OnDialogDestroying.
    if (current_dialog_model_ && current_dialog_model_->host()) {
      current_dialog_model_->host()->Close();
    }
  }

  if (navigation_source_id_ != ukm::kInvalidSourceId) {
    RecordHttpsFirstModeUKM(navigation_source_id_,
                            security_interstitials::https_only_mode::
                                BlockingResult::kInterstitialDontProceed);
    metrics_helper_->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
    navigation_source_id_ = ukm::kInvalidSourceId;
  }

  is_suspended_ = false;
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void AskBeforeHttpDialogController::CloseDialogWidget(
    views::Widget::ClosedReason reason) {
  // This is used as a callback for MakeCloseSynchronous() to catch other forms
  // of dialog closing that aren't handled elsewhere.
  if (reason == views::Widget::ClosedReason::kCancelButtonClicked) {
    // User pressed the "Continue to site" button.
    RecordHttpsFirstModeUKM(navigation_source_id_,
                            security_interstitials::https_only_mode::
                                BlockingResult::kInterstitialProceed);
    metrics_helper_->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEED);
  } else {
    // All other cases are the user not proceeding (either actively clicking "Go
    // back", or dismissing the warning for some other reason like closing the
    // tab).
    RecordHttpsFirstModeUKM(navigation_source_id_,
                            security_interstitials::https_only_mode::
                                BlockingResult::kInterstitialDontProceed);
    metrics_helper_->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
  }
  navigation_source_id_ = ukm::kInvalidSourceId;
  dialog_widget_.reset();
}
#endif

void AskBeforeHttpDialogController::OnDialogDestroying() {
#if BUILDFLAG(IS_ANDROID)
  current_dialog_model_ = nullptr;
  if (navigation_source_id_ != ukm::kInvalidSourceId) {
    // The dialog was destroyed but the user didn't dismiss it!
    // This happens on TAB_SWITCHED or SUSPENDED.
    is_suspended_ = true;
  }
#endif
}

void AskBeforeHttpDialogController::OnUserDismissed() {
#if BUILDFLAG(IS_ANDROID)
  if (current_dialog_model_) {
    ui::ModalDialogWrapper* wrapper =
        static_cast<ui::ModalDialogWrapper*>(current_dialog_model_->host());
    if (wrapper) {
      std::optional<ui::ModalDialogWrapper::DismissalCause> cause =
          wrapper->GetDismissalCause();
      // Only treat NAVIGATE_BACK and TOUCH_OUTSIDE as user dismissal. These
      // correspond to the user dismissing (but not actively making a decision)
      // on the dialog on Desktop, where we want to additionally trigger a "back
      // to safety" navigation.
      if (cause.has_value() &&
          (cause.value() ==
               ui::ModalDialogWrapper::DismissalCause::NAVIGATE_BACK ||
           cause.value() ==
               ui::ModalDialogWrapper::DismissalCause::TOUCH_OUTSIDE)) {
        OnGoBackButtonClicked();
      }
    }
  }
#endif
}

void AskBeforeHttpDialogController::OnVisibilityChanged(
    content::Visibility visibility) {
#if BUILDFLAG(IS_ANDROID)
  if (visibility == content::Visibility::VISIBLE && is_suspended_) {
    // Post a task so that the dialog is re-created after Java tab-switching
    // logic (like TabModalLifetimeHandler) has completed its cleanups.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<AskBeforeHttpDialogController> controller) {
              if (controller && controller->is_suspended_ &&
                  controller->web_contents() &&
                  controller->web_contents()->GetVisibility() ==
                      content::Visibility::VISIBLE) {
                controller->ShowDialog(controller->web_contents(),
                                       controller->request_url_,
                                       controller->navigation_source_id_);
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }
#endif
}

std::unique_ptr<ui::DialogModel>
AskBeforeHttpDialogController::CreateDialogModel(const GURL& request_url) {
  ui::DialogModel::Builder builder;
  builder.SetInternalName(kAskBeforeHttpDialogName)
      .SetIsAlertDialog()
      .SetTitle(l10n_util::GetStringUTF16(IDS_ABH_PROMPT_TITLE))
      .AddOkButton(
          base::BindOnce(&AskBeforeHttpDialogController::OnGoBackButtonClicked,
                         weak_ptr_factory_.GetWeakPtr()),
          ui::DialogModel::Button::Params()
              .SetLabel(
                  l10n_util::GetStringUTF16(IDS_HTTPS_ONLY_MODE_BACK_BUTTON))
              .SetStyle(ui::ButtonStyle::kProminent)
              .SetId(kGoBackButtonId))
#if !BUILDFLAG(IS_ANDROID)
      // This is skipped on Android because on Android we want the default
      // button set so "Go back" is prominently styled. Android keyboard focus
      // does not immediately go to the default button, so this only impacts
      // button styling.
      .OverrideDefaultButton(ui::mojom::DialogButton::kNone)
#endif
      .SetInitiallyFocusedField(kGoBackButtonId)
      .SetDialogDestroyingCallback(
          base::BindOnce(&AskBeforeHttpDialogController::OnDialogDestroying,
                         weak_ptr_factory_.GetWeakPtr()))
      .SetCloseActionCallback(
          base::BindOnce(&AskBeforeHttpDialogController::OnUserDismissed,
                         weak_ptr_factory_.GetWeakPtr()));

#if !BUILDFLAG(IS_ANDROID)
  builder.AddExtraButton(
      base::BindRepeating(
          [](base::WeakPtr<AskBeforeHttpDialogController> controller,
             const GURL& url, const ui::Event& event) {
            if (controller) {
              controller->OnContinueButtonClicked(url);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), request_url),
      ui::DialogModel::Button::Params()
          .SetLabel(
              l10n_util::GetStringUTF16(IDS_HTTPS_ONLY_MODE_SUBMIT_BUTTON))
          .SetStyle(ui::ButtonStyle::kDefault)
          .SetId(kContinueButtonId));
#else
  // Android uses Cancel button instead of Extra button.
  builder.AddCancelButton(
      base::BindOnce(&AskBeforeHttpDialogController::OnContinueButtonClicked,
                     weak_ptr_factory_.GetWeakPtr(), request_url),
      ui::DialogModel::Button::Params()
          .SetLabel(
              l10n_util::GetStringUTF16(IDS_HTTPS_ONLY_MODE_SUBMIT_BUTTON))
          .SetStyle(ui::ButtonStyle::kDefault)
          .SetId(kContinueButtonId));
#endif

  // We separately add on the main warning text, including the learn more link.
  ui::DialogModelLabel::TextReplacement link = ui::DialogModelLabel::CreateLink(
      IDS_ABH_PROMPT_LEARN_MORE_LINK,
      base::BindRepeating(
          &AskBeforeHttpDialogController::OnHelpCenterLinkClicked,
          weak_ptr_factory_.GetWeakPtr()));
  security_interstitials::https_only_mode::HttpInterstitialState
      interstitial_state =
          ComputeInterstitialState(web_contents(), request_url);

  AddAskBeforeHttpDialogText(
      builder,
      security_interstitials::https_only_mode::GetInterstitialReason(
          interstitial_state),
      link);
  return builder.Build();
}

void AskBeforeHttpDialogController::OnHelpCenterLinkClicked(
    const ui::Event& event) {
  metrics_helper_->RecordUserInteraction(
      security_interstitials::MetricsHelper::SHOW_LEARN_MORE);

  content::OpenURLParams params(
      GURL(kLearnMoreLink), content::Referrer(),
      ui::DispositionFromEventFlags(event.flags(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB),
      ui::PAGE_TRANSITION_LINK, false);
  web_contents()->OpenURL(params, /*navigation_handle_callback=*/{});
}

void AskBeforeHttpDialogController::OnGoBackButtonClicked() {
  if (HasOpenDialog()) {
    RecordHttpsFirstModeUKM(navigation_source_id_,
                            security_interstitials::https_only_mode::
                                BlockingResult::kInterstitialDontProceed);
    metrics_helper_->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
    navigation_source_id_ = ukm::kInvalidSourceId;
#if BUILDFLAG(IS_ANDROID)
    current_dialog_model_ = nullptr;
#else
    dialog_widget_.reset();
#endif
  }

  // LINT.IfChange(HttpsFirstModeGoBackLogic)
  auto& controller = web_contents()->GetController();
  if (controller.CanGoBack()) {
    controller.GoBack();
  } else {
    controller.LoadURL(chrome::ChromeUINewTabURLAsGURL(), content::Referrer(),
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  }
  // LINT.ThenChange(components/security_interstitials/content/security_interstitial_controller_client.cc:InterstitialGoBackLogic)
}

void AskBeforeHttpDialogController::OnContinueButtonClicked(
    const GURL& request_url) {
  if (HasOpenDialog()) {
    RecordHttpsFirstModeUKM(navigation_source_id_,
                            security_interstitials::https_only_mode::
                                BlockingResult::kInterstitialProceed);
    metrics_helper_->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEED);
    navigation_source_id_ = ukm::kInvalidSourceId;
#if BUILDFLAG(IS_ANDROID)
    current_dialog_model_ = nullptr;
    // Android automatically closes the dialog on button click.
#else
    dialog_widget_.reset();
#endif
  }

  // LINT.IfChange(HttpsFirstModeProceedLogic)
  content::WebContents* web_contents_ptr = web_contents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents_ptr->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());
  // StatefulSSLHostStateDelegate can be null during tests.
  if (state) {
    // Notifies the browser process when a HTTP exception is allowed in
    // HTTPS-First Mode.
    web_contents_ptr->SetAlwaysSendSubresourceNotifications();

    state->AllowHttpForHost(
        request_url.GetHost(),
        web_contents_ptr->GetPrimaryMainFrame()->GetStoragePartition());
  }
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(web_contents_ptr);
  tab_helper->set_is_navigation_upgraded(false);
  tab_helper->set_is_navigation_fallback(false);
  web_contents_ptr->GetController().Reload(content::ReloadType::NORMAL, false);
  // The failed https navigation will remain as a forward entry, so it needs to
  // be removed.
  web_contents_ptr->GetController().PruneForwardEntries();
  // LINT.ThenChange(chrome/browser/ssl/https_only_mode_controller_client.cc:HttpsFirstModeProceedLogic)
}
