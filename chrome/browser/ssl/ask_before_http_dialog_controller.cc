// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/class_property.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/bulleted_label_list/bulleted_label_list_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"

using HttpWarningReason =
    security_interstitials::https_only_mode::InterstitialReason;

namespace {

inline constexpr char kLearnMoreLink[] =
    "https://support.google.com/chrome?p=first_mode";

// Helper to create the prompt body view based on the Ask-before-HTTP warning
// type.
void AddAskBeforeHttpDialogText(ui::DialogModel& dialog_model,
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
    // Default text includes parts as a bulleted list.
    // TODO(crbug.com/351990829): Replace this with a custom implementation.
    // The existing BulletedLabelListView is pretty minimal and doesn't allow
    // much flexibility. Having our own would allow more control over margins,
    // line heights, text context/styling, and may make it easier to bold
    // certain parts (i.e., uses StyledLabel and exposes them for
    // customization).
    // TODO(crbug.com/351990829): On Android, we just want to add each bit of
    // text as a separate paragraph, so the dialog bridge can port it
    // directly. We also want to have the "Continue" button be the "cancel"
    // button.
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

AskBeforeHttpDialogController::AskBeforeHttpDialogController(
    tabs::TabInterface* tab_interface)
    : tab_interface_(tab_interface) {
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
  tab_will_detach_subscription_ = tab_interface_->RegisterWillDetach(
      base::BindRepeating(&AskBeforeHttpDialogController::TabWillDetach,
                          base::Unretained(this)));
}

AskBeforeHttpDialogController::~AskBeforeHttpDialogController() {
  if (HasOpenDialogWidget()) {
    CloseDialogWidget(views::Widget::ClosedReason::kUnspecified);
  }
}

void AskBeforeHttpDialogController::ShowDialog(
    content::WebContents* web_contents,
    const GURL& request_url,
    ukm::SourceId navigation_source_id) {
  // If we are triggering a new dialog, that means another navigation
  // finished, so we should prefer to replace the dialog and cancel
  // the old one. TabDialogManager handles this for us.

  // The widget will own `model_host` through DialogDelegate.
  views::BubbleDialogModelHost* model_host =
      views::BubbleDialogModelHost::CreateModal(CreateDialogModel(request_url),
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

  // Track the source ID for the navigation that triggered the dialog.
  navigation_source_id_ = navigation_source_id;

  metrics_helper_->RecordUserDecision(
      security_interstitials::MetricsHelper::SHOW);
  metrics_helper_->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);

  dialog_widget_ =
      tab_interface_->GetTabFeatures()
          ->tab_dialog_manager()
          ->CreateAndShowDialog(model_host, std::move(tab_dialog_params));
  dialog_widget_->MakeCloseSynchronous(
      base::BindOnce(&AskBeforeHttpDialogController::CloseDialogWidget,
                     weak_ptr_factory_.GetWeakPtr()));
  // By default, the dialog may not have its initially focused view
  // actually focused on some platforms (see crbug.com/440104083).
  // Explicitly call RequestFocus() to ensure this happens.
  views::View* focused_view = model_host->GetInitiallyFocusedView();
  CHECK(focused_view);
  focused_view->RequestFocus();
}

bool AskBeforeHttpDialogController::HasOpenDialogWidget() const {
  return dialog_widget_ && !dialog_widget_->IsClosed();
}

void AskBeforeHttpDialogController::CloseDialogWidget(
    views::Widget::ClosedReason reason) {
  // NOTE: Losing focus (e.g., switching away from the tab with the dialog)
  // does not cause the widget to be closed.
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

std::unique_ptr<ui::DialogModel>
AskBeforeHttpDialogController::CreateDialogModel(const GURL& request_url) {
  auto dialog_model =
      ui::DialogModel::Builder()
          .SetInternalName(kAskBeforeHttpDialogName)
          // Make screen readers announce the contents of the dialog when it
          // appears.
          .SetIsAlertDialog()
          .SetTitle(l10n_util::GetStringUTF16(IDS_ABH_PROMPT_TITLE))
          // TODO(crbug.com/351990829): On Android this should just use
          // AddCancelButton().
          .AddExtraButton(
              base::BindRepeating(
                  &AskBeforeHttpDialogController::OnContinueButtonClicked,
                  weak_ptr_factory_.GetWeakPtr(), request_url),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_HTTPS_ONLY_MODE_SUBMIT_BUTTON))
                  .SetStyle(ui::ButtonStyle::kDefault)
                  .SetId(kContinueButtonId))
          .AddOkButton(
              base::BindOnce(
                  &AskBeforeHttpDialogController::OnGoBackButtonClicked,
                  weak_ptr_factory_.GetWeakPtr()),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_HTTPS_ONLY_MODE_BACK_BUTTON))
                  .SetStyle(ui::ButtonStyle::kProminent)
                  .SetId(kGoBackButtonId))
          .OverrideDefaultButton(ui::mojom::DialogButton::kNone)
          .SetInitiallyFocusedField(kGoBackButtonId)
          .Build();

  // We separately add on the main warning text, including the learn more link.
  ui::DialogModelLabel::TextReplacement link = ui::DialogModelLabel::CreateLink(
      IDS_ABH_PROMPT_LEARN_MORE_LINK,
      base::BindRepeating(
          &AskBeforeHttpDialogController::OnHelpCenterLinkClicked,
          weak_ptr_factory_.GetWeakPtr()));
  security_interstitials::https_only_mode::HttpInterstitialState
      interstitial_state =
          ComputeInterstitialState(tab_interface_->GetContents(), request_url);

  AddAskBeforeHttpDialogText(
      *dialog_model,
      security_interstitials::https_only_mode::GetInterstitialReason(
          interstitial_state),
      link);
  return dialog_model;
}

void AskBeforeHttpDialogController::OnHelpCenterLinkClicked(
    const ui::Event& event) {
  metrics_helper_->RecordUserInteraction(
      security_interstitials::MetricsHelper::SHOW_LEARN_MORE);

  tab_interface_->GetBrowserWindowInterface()->OpenGURL(
      GURL(kLearnMoreLink),
      ui::DispositionFromEventFlags(event.flags(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

void AskBeforeHttpDialogController::OnGoBackButtonClicked() {
  if (HasOpenDialogWidget()) {
    CloseDialogWidget(views::Widget::ClosedReason::kAcceptButtonClicked);
  }

  // LINT.IfChange(HttpsFirstModeGoBackLogic)
  auto& controller = tab_interface_->GetContents()->GetController();
  if (controller.CanGoBack()) {
    controller.GoBack();
  } else {
    controller.LoadURL(GURL(chrome::kChromeUINewTabURL), content::Referrer(),
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  }
  // LINT.ThenChange(components/security_interstitials/content/security_interstitial_controller_client.cc:InterstitialGoBackLogic)
}

void AskBeforeHttpDialogController::OnContinueButtonClicked(
    const GURL& request_url,
    const ui::Event& event) {
  if (HasOpenDialogWidget()) {
    CloseDialogWidget(views::Widget::ClosedReason::kCancelButtonClicked);
  }

  // LINT.IfChange(HttpsFirstModeProceedLogic)
  content::WebContents* web_contents = tab_interface_->GetContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());
  // StatefulSSLHostStateDelegate can be null during tests.
  if (state) {
    // Notifies the browser process when a HTTP exception is allowed in
    // HTTPS-First Mode.
    web_contents->SetAlwaysSendSubresourceNotifications();

    state->AllowHttpForHost(
        request_url.GetHost(),
        web_contents->GetPrimaryMainFrame()->GetStoragePartition());
  }
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(web_contents);
  tab_helper->set_is_navigation_upgraded(false);
  tab_helper->set_is_navigation_fallback(false);
  web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
  // The failed https navigation will remain as a forward entry, so it needs to
  // be removed.
  web_contents->GetController().PruneForwardEntries();
  // LINT.ThenChange(chrome/browser/ssl/https_only_mode_controller_client.cc:HttpsFirstModeProceedLogic)
}

void AskBeforeHttpDialogController::TabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete &&
      HasOpenDialogWidget()) {
    // TODO(crbug.com/351990829): Consider adding a new `ClosedReason`
    // value for this case.
    CloseDialogWidget(views::Widget::ClosedReason::kUnspecified);
  }
}
