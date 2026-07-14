// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/prefs/pref_service.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

// The maximum length of the selection text sent as a suggested prompt.
// Selections longer than this are ignored.
// We send a truncated version of the selection to the panel as it does not
// need to be the full text.
constexpr size_t kMaxSelectionLength = 1000;
constexpr size_t kMaxSelectionLengthSentToPanel = 100;

// The minimum length of the selection text sent as a suggested prompt.
// Selections shorter than this are ignored.
constexpr size_t kMinSelectionLength = 3;

// The MIME type for selected text.
constexpr char kSelectionMimeType[] = "application/x-glic-selection";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicSelectionAction)
enum class GlicSelectionAction {
  kNudgeShown = 0,
  kWidgetShown = 1,
  kNudgeClicked = 2,
  kWidgetClicked = 3,
  kMaxValue = kWidgetClicked
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicSelectionAction)



mojom::AdditionalContextPtr CreateAdditionalContext(
    content::WebContents* web_contents,
    const std::u16string& selected_text) {
  auto context = mojom::AdditionalContext::New();
  context->source = mojom::AdditionalContextSource::kTextSelection;
  std::vector<mojom::AdditionalContextPartPtr> parts;
  if (!selected_text.empty()) {
    auto context_data = mojom::ContextData::New();
    context_data->mime_type = kSelectionMimeType;
    std::string utf8_text = base::UTF16ToUTF8(
        selected_text.substr(0, kMaxSelectionLengthSentToPanel));
    context_data->data =
        mojo_base::BigBuffer(base::as_bytes(base::span(utf8_text)));
    parts.push_back(
        mojom::AdditionalContextPart::NewData(std::move(context_data)));
  }
  if (auto* tab_interface =
          tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    context->tab_id = tab_interface->GetHandle().raw_value();
  }
  context->parts = std::move(parts);
  return context;
}

bool IsListenedToInputEvent(blink::WebInputEvent::Type type) {
  switch (type) {
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kPointerDown:
    case blink::WebInputEvent::Type::kGestureTapDown:
    case blink::WebInputEvent::Type::kTouchStart:
    case blink::WebInputEvent::Type::kMouseUp:
    case blink::WebInputEvent::Type::kPointerUp:
    case blink::WebInputEvent::Type::kPointerCancel:
    case blink::WebInputEvent::Type::kTouchEnd:
    case blink::WebInputEvent::Type::kTouchCancel:
    case blink::WebInputEvent::Type::kGestureTapCancel:
    case blink::WebInputEvent::Type::kKeyUp:
    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyDown:
    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kMouseWheel:
      return true;
    default:
      return false;
  }
}

}  // namespace

class GlicSelectionObserver::WidgetActionDelegate
    : public GlicSelectionWidgetDelegate::ActionDelegate {
 public:
  explicit WidgetActionDelegate(GlicSelectionObserver* observer)
      : observer_(observer) {}

  // GlicSelectionWidgetDelegate::ActionDelegate:
  void OnAskGemini() override { observer_->OnAskGemini(); }
  void OnCopy() override { observer_->OnCopy(); }
  void OnCopyLink() override { observer_->OnCopyLink(); }
  void OnHideForThisSite() override { observer_->OnHideForThisSite(); }
  void OnSettings() override { observer_->OnSettings(); }
  void OnWidgetClose() override { observer_->OnWidgetClose(); }

 private:
  raw_ptr<GlicSelectionObserver> observer_;
};

GlicSelectionObserver::GlicSelectionObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      action_delegate_(std::make_unique<WidgetActionDelegate>(this)) {
  CHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  glic_keyed_service_ = GlicKeyedService::Get(profile);

  if (glic_keyed_service_) {
    panel_state_subscription_ =
        glic_keyed_service_->instance_coordinator().AddGlobalShowHideCallback(
            base::BindRepeating(&GlicSelectionObserver::OnGlobalPanelShowHide,
                                weak_ptr_factory_.GetWeakPtr()));
  }

  std::string account;
  if (profile) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    if (identity_manager) {
      account =
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .email;
    }
  }

  auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(web_contents);
  auto* helper = tab_interface
                     ? tabs::PageContextEligibilityHelper::From(tab_interface)
                     : nullptr;
  if (helper) {
    page_context_eligibility_subscription_ =
        helper->RegisterEligibilityChangeCallback(base::BindRepeating(
            &GlicSelectionObserver::OnPageContextEligibilityChanged,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    CreatePageContextEligibilityAPI(std::move(account));
  }

  web_contents->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        RenderFrameCreated(render_frame_host);
      });
}

bool GlicSelectionObserver::IsSelectionPromptEnabled() const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return GlicEnabling::IsSelectionPromptEnabledForProfile(profile);
}

GlicSelectionObserver::~GlicSelectionObserver() {
  widget_delegate_.reset();

  base::flat_set<content::RenderWidgetHost*> unique_rwhs;
  for (const auto& frame_token : observed_frames_) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromFrameToken(frame_token);
    if (rfh && rfh->GetRenderWidgetHost()) {
      unique_rwhs.insert(rfh->GetRenderWidgetHost());
    }
  }
  for (auto* rwh : unique_rwhs) {
    rwh->RemoveInputEventObserver(this);
  }
  observed_frames_.clear();
}

void GlicSelectionObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (auto* rwh = render_frame_host->GetRenderWidgetHost()) {
    bool already_observing = false;
    for (const auto& frame_token : observed_frames_) {
      content::RenderFrameHost* rfh =
          content::RenderFrameHost::FromFrameToken(frame_token);
      if (rfh && rfh->GetRenderWidgetHost() == rwh) {
        already_observing = true;
        break;
      }
    }
    if (observed_frames_.insert(render_frame_host->GetGlobalFrameToken())
            .second) {
      if (!already_observing) {
        rwh->AddInputEventObserver(this);
      }
    }
  }
}

void GlicSelectionObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!observed_frames_.contains(render_frame_host->GetGlobalFrameToken())) {
    return;
  }

  content::RenderWidgetHost* rwh = render_frame_host->GetRenderWidgetHost();
  observed_frames_.erase(render_frame_host->GetGlobalFrameToken());

  bool still_observing = false;
  for (const auto& frame_token : observed_frames_) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromFrameToken(frame_token);
    if (rfh && rfh->GetRenderWidgetHost() == rwh) {
      still_observing = true;
      break;
    }
  }
  if (!still_observing && rwh) {
    rwh->RemoveInputEventObserver(this);
  }
}
void GlicSelectionObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN && widget_delegate_) {
    widget_delegate_->CloseWidget();
  }
}

void GlicSelectionObserver::PrimaryPageChanged(content::Page& page) {
  is_hidden_on_current_page_ = false;
  if (widget_delegate_) {
    widget_delegate_->CloseWidget();
  }
}

void GlicSelectionObserver::PrimaryMainFrameWasResized(bool width_changed) {
  DismissUI(/*keep_nudge=*/true);
}

void GlicSelectionObserver::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  if (web_contents()->IsBeingDestroyed()) {
    ResetPendingSelection();
    return;
  }

  // If the web contents loses focus, process any pending selection immediately.
  ProcessPendingSelection();
}

void GlicSelectionObserver::OnInputEvent(
    const content::RenderWidgetHost& host,
    const blink::WebInputEvent& event,
    content::RenderWidgetHost::InputEventObserver::InputEventSource source) {
  if (!IsListenedToInputEvent(event.GetType())) {
    return;
  }
  if (!IsTabValidForSharing(web_contents())) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&GlicSelectionObserver::ProcessInputEvent,
                                weak_ptr_factory_.GetWeakPtr(), event.Clone()));
}

void GlicSelectionObserver::ProcessInputEvent(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (!IsSelectionPromptEnabled()) {
    return;
  }

  switch (event->GetType()) {
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kPointerDown:
    case blink::WebInputEvent::Type::kGestureTapDown:
    case blink::WebInputEvent::Type::kTouchStart: {
      bool is_left_click_or_touch = true;
      if (event->GetType() == blink::WebInputEvent::Type::kMouseDown ||
          event->GetType() == blink::WebInputEvent::Type::kPointerDown) {
        const auto& mouse_event =
            static_cast<const blink::WebMouseEvent&>(*event);
        if (mouse_event.button != blink::WebPointerProperties::Button::kLeft) {
          is_left_click_or_touch = false;
        }
      }

      is_key_selection_ = false;
      bounds_retry_count_ = 0;
      DismissUI(/*keep_nudge=*/false);

      // Workaround for a bug in Blink: when a user single-clicks directly on
      // top of an existing selection, Blink collapses the selection on MouseUp
      // but fails to send the corresponding OnTextSelectionChanged(empty) IPC.
      // Since any left-click or touch tap invalidates the current static text
      // selection (by either placing the caret, clearing the selection, or
      // initiating a new drag), we preemptively clear the context here to
      // ensure it is not left hanging.
      if (is_left_click_or_touch) {
        is_selecting_ = true;
        ResetPendingSelection();
        if (has_sent_selection_context_) {
          UpdateSelectionState(std::u16string(),
                               /*is_pending_selection=*/false);
        }
      }
      break;
    }

    case blink::WebInputEvent::Type::kMouseUp:
    case blink::WebInputEvent::Type::kPointerUp:
    case blink::WebInputEvent::Type::kPointerCancel:
    case blink::WebInputEvent::Type::kTouchEnd:
    case blink::WebInputEvent::Type::kTouchCancel:
    case blink::WebInputEvent::Type::kGestureTapCancel:
      // Process the selection received so far. If the final selection IPC is
      // delayed, OnTextSelectionChanged will handle it since `is_selecting_`
      // becomes false.
      ProcessPendingSelection();
      break;

    case blink::WebInputEvent::Type::kKeyUp:
      if (is_key_selection_) {
        ProcessPendingSelection();
      }
      break;

    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyDown: {
      if (is_key_selection_) {
        break;
      }
      DismissUI(/*keep_nudge=*/false);
      const auto& keyboard_event =
          static_cast<const blink::WebKeyboardEvent&>(*event);
#if BUILDFLAG(IS_MAC)
      int select_all_modifier = blink::WebInputEvent::Modifiers::kMetaKey;
#else
      int select_all_modifier = blink::WebInputEvent::Modifiers::kControlKey;
#endif
      bool is_select_all = (event->GetModifiers() & select_all_modifier) &&
                           keyboard_event.windows_key_code == ui::VKEY_A;
      bool is_shift =
          event->GetModifiers() & blink::WebInputEvent::Modifiers::kShiftKey;
      bool is_navigation_key =
          keyboard_event.windows_key_code == ui::VKEY_LEFT ||
          keyboard_event.windows_key_code == ui::VKEY_RIGHT ||
          keyboard_event.windows_key_code == ui::VKEY_UP ||
          keyboard_event.windows_key_code == ui::VKEY_DOWN ||
          keyboard_event.windows_key_code == ui::VKEY_HOME ||
          keyboard_event.windows_key_code == ui::VKEY_END ||
          keyboard_event.windows_key_code == ui::VKEY_PRIOR ||
          keyboard_event.windows_key_code == ui::VKEY_NEXT;
      if ((is_shift && is_navigation_key) || is_select_all) {
        is_key_selection_ = true;
        is_selecting_ = true;
        if (!last_selected_text_.empty()) {
          pending_selection_text_ = last_selected_text_;
        } else {
          ResetPendingSelection();
        }
      }
      break;
    }

    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kMouseWheel:
      DismissUI(/*keep_nudge=*/true);
      break;

    default:
      break;
  }
}

void GlicSelectionObserver::OnTextSelectionChanged(
    content::RenderFrameHost* render_frame_host,
    std::u16string_view selected_text) {
  if (!IsSelectionPromptEnabled()) {
    return;
  }

  if (!IsTabValidForSharing(web_contents())) {
    return;
  }

  if (web_contents()->IsFocusedElementEditable()) {
    selected_text = std::u16string_view();
  }

  bounds_retry_count_ = 0;

  std::u16string_view trimmed_text =
      base::TrimWhitespace(selected_text, base::TRIM_ALL);

  if (trimmed_text.length() > kMaxSelectionLength) {
    pending_selection_text_ = std::u16string();
  } else {
    size_t non_whitespace_count = 0;
    bool exceeds_minimum_selection_length = false;
    for (char16_t c : trimmed_text) {
      if (!base::IsUnicodeWhitespace(c)) {
        non_whitespace_count++;
      }
      if (non_whitespace_count == kMinSelectionLength) {
        exceeds_minimum_selection_length = true;
        break;
      }
    }

    if (!exceeds_minimum_selection_length) {
      pending_selection_text_ = std::u16string();
    } else {
      pending_selection_text_ = std::u16string(trimmed_text);
    }
  }
  if (render_frame_host) {
    last_selection_frame_token_ = render_frame_host->GetGlobalFrameToken();
  }

  // If not in the process of selecting, process the selection immediately.
  if (!is_selecting_) {
    ProcessPendingSelection();
  }
}

void GlicSelectionObserver::DismissUI(bool keep_nudge) {
  if (widget_delegate_) {
    widget_delegate_->CloseWidget();
  }
}

void GlicSelectionObserver::ProcessPendingSelection() {
  is_selecting_ = false;
  is_key_selection_ = false;
  if (!pending_selection_text_.has_value()) {
    return;
  }

  std::u16string selected_text = std::move(*pending_selection_text_);
  ResetPendingSelection();

  UpdateSelectionState(selected_text, /*is_pending_selection=*/true);
}

void GlicSelectionObserver::ResetPendingSelection() {
  pending_selection_text_.reset();
}

// static
void GlicSelectionObserver::InvokeGlicFromSelectionAffordance(
    std::u16string selected_text,
    bool is_widget,
    base::WeakPtr<content::WebContents> web_contents,
    GlicNudgeActivity activity) {
  if (activity != GlicNudgeActivity::kNudgeClicked) {
    return;
  }

  bool is_post_fre = false;
  if (web_contents) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    is_post_fre = GlicEnabling::HasConsentedForProfile(profile);
  }

  const char* histogram_suffix = is_post_fre ? ".PostFre" : ".PreFre";

  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Selection.Action", histogram_suffix}),
      is_widget ? GlicSelectionAction::kWidgetClicked
                : GlicSelectionAction::kNudgeClicked);
  if (is_widget) {
    base::UmaHistogramCounts1000(
        base::StrCat(
            {"Glic.Selection.WidgetClicked.SelectionLength", histogram_suffix}),
        selected_text.length());
  } else {
    base::UmaHistogramCounts1000(
        base::StrCat(
            {"Glic.Selection.NudgeClicked.SelectionLength", histogram_suffix}),
        selected_text.length());
  }

  if (web_contents) {
    if (auto* tab_interface =
            tabs::TabInterface::MaybeGetFromContents(web_contents.get())) {
      if (tab_interface->GetBrowserWindowInterface()) {
        Profile* profile =
            Profile::FromBrowserContext(web_contents->GetBrowserContext());
        if (auto* glic_keyed_service = GlicKeyedService::Get(profile)) {
          GlicInvokeOptions options(glic::Target(*tab_interface),
                                    mojom::InvocationSource::kNudge);
          options.additional_context = AdditionalTabContext(
              CreateAdditionalContext(web_contents.get(), selected_text),
              content::GlobalRenderFrameHostId(), PolicyCheck::kNone);
          if (features::kGlicSelectionAutoSendPrompt.Get()) {
            std::string cta = features::kGlicSelectionPromptCta.Get();
            std::string prompt = l10n_util::GetStringUTF8(
                IDS_GLIC_SELECTION_AUTO_SEND_PROMPT_TELL_ME);
            if (cta == features::kGlicSelectionPromptCtaExplain) {
              prompt = l10n_util::GetStringUTF8(
                  IDS_GLIC_SELECTION_AUTO_SEND_PROMPT_EXPLAIN);
            }
            options.prompts.push_back(prompt);
            glic_keyed_service->InvokeWithAutoSubmit(
                InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
                std::move(options));
          } else {
            glic_keyed_service->Invoke(std::move(options));
          }
        }
      }
    }
  }
}

void GlicSelectionObserver::UpdateSelectionState(
    const std::u16string& selected_text,
    bool is_pending_selection) {
  last_selected_text_ = selected_text;
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (!tab_interface) {
    return;
  }
  BrowserWindowInterface* bwi = tab_interface->GetBrowserWindowInterface();

  if (selected_text.empty()) {
    if (widget_delegate_) {
      widget_delegate_->CloseWidget();
    }

    if (has_sent_selection_context_) {
      SendAdditionalContextToPanel(tab_interface, u"");
      has_sent_selection_context_ = false;
    }

    return;
  }

  if (!bwi) {
    return;
  }

  bool panel_showing = IsPanelShowing(tab_interface, bwi);

  if (panel_showing) {
    if (is_pending_selection &&
        !features::kGlicSelectionPromptUpdatesOnly.Get()) {
      ShowSelectionAffordance(selected_text, bwi);
    } else if (widget_delegate_) {
      widget_delegate_->CloseWidget();
    }

    // TODO(b/508916357): Use the invoke API.
    SendAdditionalContextToPanel(tab_interface, selected_text);
    has_sent_selection_context_ = true;
  } else {
    if (is_pending_selection &&
        !features::kGlicSelectionPromptUpdatesOnly.Get()) {
      ShowSelectionAffordance(selected_text, bwi);
    }
    has_sent_selection_context_ = false;
  }
}

void GlicSelectionObserver::ShowSelectionAffordance(
    const std::u16string& selected_text,
    BrowserWindowInterface* bwi) {
  auto* controller = bwi->GetFeatures().glic_nudge_controller();
  if (controller) {
    bool is_post_fre = GlicEnabling::HasConsentedForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    const char* histogram_suffix = is_post_fre ? ".PostFre" : ".PreFre";

    // Show selection widget
    if (!ShouldShowSelectionWidget()) {
      return;
    }
    // Find the RenderFrameHost that has the selection.
    content::RenderFrameHost* selected_frame =
        last_selection_frame_token_.has_value()
            ? content::RenderFrameHost::FromFrameToken(
                  *last_selection_frame_token_)
            : nullptr;
    if (!selected_frame) {
      return;
    }

    std::optional<gfx::Rect> bounds =
        web_contents()->GetTextSelectionBounds(selected_frame);
    if (bounds.has_value() && !bounds->IsEmpty()) {
      if (widget_delegate_) {
        widget_delegate_->CloseWidget();
      }

      base::UmaHistogramEnumeration(
          base::StrCat({"Glic.Selection.Action", histogram_suffix}),
          GlicSelectionAction::kWidgetShown);

      widget_delegate_ = std::make_unique<GlicSelectionWidgetDelegate>(
          *action_delegate_, *bounds, web_contents()->GetContainerBounds(),
          std::u16string(selected_text));
      widget_delegate_->set_parent_window(platform_util::GetViewForWindow(
          web_contents()->GetTopLevelNativeWindow()));
      widget_delegate_->ShowWidget();
      if (features::kGlicSelectionShowCopyButtons.Get()) {
        RequestLinkGeneration(selected_frame);
      }
    } else if (bounds_retry_count_ < 5) {
      // Retry showing the widget, bounds might not be available yet due
      // to IPC timing (especially on double click).
      bounds_retry_count_++;
      pending_selection_text_ = selected_text;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&GlicSelectionObserver::ProcessPendingSelection,
                         weak_ptr_factory_.GetWeakPtr()),
          base::Milliseconds(100));
    }
  }
}

bool GlicSelectionObserver::ShouldShowSelectionWidget() {
  // TODO(b/519247911): Update this.
  if (is_hidden_on_current_page_) {
    return false;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  if (ContentSettingsPattern::FromURL(web_contents()->GetLastCommittedURL())
          .IsValid()) {
    HostContentSettingsMap* settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile);
    ContentSetting setting =
        settings_map->GetContentSetting(web_contents()->GetLastCommittedURL(),
                                        web_contents()->GetLastCommittedURL(),
                                        ContentSettingsType::INLINE_CUE_MENU);
    if (setting == CONTENT_SETTING_BLOCK) {
      return false;
    }
  }

  // Check the top cue only list.
  std::string top_cue_only_list_str =
      features::kGlicSelectionTopCueOnlyList.Get();
  if (!top_cue_only_list_str.empty()) {
    std::vector<std::string> top_cue_only_hosts =
        base::SplitString(top_cue_only_list_str, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    std::string_view current_host =
        web_contents()->GetLastCommittedURL().host();
    for (const std::string& host : top_cue_only_hosts) {
      if (current_host == host || current_host.ends_with("." + host)) {
        return false;
      }
    }
  }

  return true;
}

void GlicSelectionObserver::OnHideForThisSite() {
  is_hidden_on_current_page_ = true;

  if (ContentSettingsPattern::FromURL(web_contents()->GetLastCommittedURL())
          .IsValid()) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    HostContentSettingsMap* settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetLastCommittedURL(), GURL(),
        ContentSettingsType::INLINE_CUE_MENU, CONTENT_SETTING_BLOCK);
  }

  DismissUI(/*keep_nudge=*/false);
  ShowHiddenToast(ToastId::kGlicSelectionHiddenForSite);
}

void GlicSelectionObserver::ShowHiddenToast(ToastId toast_id) {
  if (auto* toast_controller =
          ToastController::MaybeGetForWebContents(web_contents())) {
    toast_controller->MaybeShowToast(ToastParams(toast_id));
  }
}

void GlicSelectionObserver::OnSettings() {
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (tab_interface) {
    BrowserWindowInterface* browser_window_interface =
        tab_interface->GetBrowserWindowInterface();
    if (browser_window_interface) {
      chrome::ShowContentSettingsExceptions(
          browser_window_interface, ContentSettingsType::INLINE_CUE_MENU);
    }
  }
}


void GlicSelectionObserver::RequestLinkGeneration(
    content::RenderFrameHost* rfh) {
  generated_link_.reset();
  if (!rfh) {
    return;
  }

  GURL url = rfh->GetMainFrame()->GetLastCommittedURL();
  if (url.has_ref()) {
    url = shared_highlighting::RemoveFragmentSelectorDirectives(url);
  }

  if (!shared_highlighting::ShouldOfferLinkToText(url)) {
    return;
  }

  text_fragment_remote_.reset();
  rfh->GetRemoteInterfaces()->GetInterface(
      text_fragment_remote_.BindNewPipeAndPassReceiver());

  text_fragment_remote_->RequestSelectorForSelection(
      base::BindOnce(&GlicSelectionObserver::OnLinkGenerated,
                     weak_ptr_factory_.GetWeakPtr(), url));
}

void GlicSelectionObserver::WriteLinkToClipboard(
    content::WeakDocumentPtr weak_document_ptr,
    const GURL& url) {
  content::RenderFrameHost* rfh = weak_document_ptr.AsRenderFrameHostIfValid();
  if (!rfh) {
    return;
  }

  enterprise_data_protection::CopyTextToClipboard(
      rfh, base::UTF8ToUTF16(url.spec()));

  if (auto* web_contents_ptr = content::WebContents::FromRenderFrameHost(rfh)) {
    shared_highlighting::LogDesktopLinkGenerationCopiedLinkType(
        shared_highlighting::LinkGenerationCopiedLinkType::
            kCopiedFromNewGeneration);

    if (toast_features::IsEnabled(
            toast_features::kLinkToHighlightCopiedToast)) {
      if (auto* tab_interface =
              tabs::TabInterface::MaybeGetFromContents(web_contents_ptr)) {
        if (auto* bwi = tab_interface->GetBrowserWindowInterface()) {
          if (auto* toast_controller = bwi->GetFeatures().toast_controller()) {
            toast_controller->MaybeShowToast(
                ToastParams(ToastId::kLinkToHighlightCopied));
          }
        }
      }
    }

    feature_engagement::TrackerFactory::GetForBrowserContext(
        web_contents_ptr->GetBrowserContext())
        ->NotifyEvent("iph_desktop_shared_highlighting_used");
  }
}

void GlicSelectionObserver::OnLinkGenerated(
    const GURL& fallback_url,
    const std::string& selector,
    shared_highlighting::LinkGenerationError error,
    shared_highlighting::LinkGenerationReadyStatus ready_status) {
  if (!selector.empty()) {
    generated_link_ =
        shared_highlighting::AppendSelectors(fallback_url, {selector});
  }
  if (widget_delegate_) {
    widget_delegate_->UpdateCopyLinkButton(generated_link_.has_value());
  }
}

bool GlicSelectionObserver::IsPanelShowing(tabs::TabInterface* tab_interface,
                                           BrowserWindowInterface* bwi) {
  if (glic_keyed_service_ &&
      glic_keyed_service_->GetInstanceForTab(tab_interface)) {
    return glic_keyed_service_->IsPanelShowingForBrowser(*bwi);
  }
  return false;
}

void GlicSelectionObserver::SendAdditionalContextToPanel(
    tabs::TabInterface* tab_interface,
    const std::u16string& selected_text) {
  if (!glic_keyed_service_) {
    return;
  }

  // If the page is not eligible, do not send the additional context.
  if (!IsPageContextEligible() && !selected_text.empty()) {
    return;
  }

  if (auto* instance = glic_keyed_service_->GetInstanceForTab(tab_interface)) {
    // TODO(b/508916357): Use the invoke API.
    instance->SendAdditionalContext(
        CreateAdditionalContext(web_contents(), selected_text));
  }
}

bool GlicSelectionObserver::IsPageContextEligible() const {
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (tab_interface) {
    auto* helper = tabs::PageContextEligibilityHelper::From(tab_interface);
    if (helper) {
      return helper->IsPageContextEligible() ==
             optimization_guide::PageContextEligibilityStatus::kEligible;
    }
  }
  if (page_context_tracker_) {
    return page_context_tracker_->IsPageContextEligible() ==
           optimization_guide::PageContextEligibilityStatus::kEligible;
  }
  return false;
}

void GlicSelectionObserver::OnPageContextEligibilityChanged(
    optimization_guide::PageContextEligibilityStatus status) {
  // If the page context transitions into a liminal state (kUnknown) or becomes
  // ineligible (kNotEligible), and we've already sent selection context, we
  // should clear it.
  if (status != optimization_guide::PageContextEligibilityStatus::kEligible &&
      has_sent_selection_context_) {
    auto* tab_interface =
        tabs::TabInterface::MaybeGetFromContents(web_contents());
    if (tab_interface) {
      SendAdditionalContextToPanel(tab_interface, std::u16string());
      has_sent_selection_context_ = false;
    }
  }
}

void GlicSelectionObserver::CreatePageContextEligibilityAPI(
    std::string account) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&optimization_guide::PageContextEligibility::Get),
      base::BindOnce(&GlicSelectionObserver::OnPageContextEligibilityAPILoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(account)));
}

void GlicSelectionObserver::OnPageContextEligibilityAPILoaded(
    std::string account,
    optimization_guide::PageContextEligibility* page_context_eligibility) {
  if (!page_context_eligibility) {
    return;
  }
  page_context_tracker_ =
      optimization_guide::PageContextEligibilityObserver::Create(
          web_contents(), std::move(account),
          base::BindRepeating(
              [](base::WeakPtr<GlicSelectionObserver> observer,
                 optimization_guide::PageContextEligibilityStatus status) {
                if (observer) {
                  observer->OnPageContextEligibilityChanged(status);
                }
              },
              weak_ptr_factory_.GetWeakPtr()));
}

void GlicSelectionObserver::CopyLinkToHighlight(
    content::WeakDocumentPtr weak_document_ptr) {
  if (generated_link_.has_value() && generated_link_->is_valid()) {
    WriteLinkToClipboard(weak_document_ptr, generated_link_.value());
  }
}

void GlicSelectionObserver::OnGlobalPanelShowHide() {
  if (last_selected_text_.empty()) {
    return;
  }

  UpdateSelectionState(last_selected_text_, /*is_pending_selection=*/false);
}

void GlicSelectionObserver::OnAskGemini() {
  DismissUI(/*keep_nudge=*/false);
  InvokeGlicFromSelectionAffordance(last_selected_text_, /*is_widget=*/true,
                                    web_contents()->GetWeakPtr(),
                                    GlicNudgeActivity::kNudgeClicked);
}

void GlicSelectionObserver::OnCopy() {
  DismissUI(/*keep_nudge=*/false);
  web_contents()->Copy();
}

void GlicSelectionObserver::OnCopyLink() {
  DismissUI(/*keep_nudge=*/false);
  content::RenderFrameHost* selected_frame =
      last_selection_frame_token_.has_value()
          ? content::RenderFrameHost::FromFrameToken(
                *last_selection_frame_token_)
          : nullptr;
  if (selected_frame) {
    CopyLinkToHighlight(selected_frame->GetWeakDocumentPtr());
  }
}

void GlicSelectionObserver::OnWidgetClose() {
  if (widget_delegate_) {
    // Defer the destruction of the delegate to ensure the views::Widget is
    // destroyed first, and then the delegate. This is required under the
    // CLIENT_OWNS_WIDGET ownership model.
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(widget_delegate_));
  }
}

}  // namespace glic
