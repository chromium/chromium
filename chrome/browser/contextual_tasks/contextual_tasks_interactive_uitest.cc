// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_interactive_test_base.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_test_user_variation.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/host_override.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/page_info/page_info.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/aim_query.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"

using testing::_;

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGenericTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGenericTab2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInnerWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementDoesNotExistEvent);
using MockContextualTasksEligibilityManager =
    contextual_tasks::TestContextualTasksEligibilityManager;
using MockContextualTasksUiService =
    contextual_tasks::TestContextualTasksUiService;

class ClipboardTextObserver
    : public ui::test::ObservationStateObserver<std::u16string,
                                                ui::ClipboardMonitor,
                                                ui::ClipboardObserver> {
 public:
  explicit ClipboardTextObserver(ui::ClipboardMonitor* clipboard_monitor)
      : ObservationStateObserver<std::u16string,
                                 ui::ClipboardMonitor,
                                 ui::ClipboardObserver>(clipboard_monitor) {
    PollClipboard();
  }
  ~ClipboardTextObserver() override = default;

  // ObservationStateObserver:
  std::u16string GetStateObserverInitialState() const override {
    return std::u16string();
  }

  // ClipboardObserver:
  void OnClipboardDataChanged() override { PollClipboard(); }

 private:
  void PollClipboard() {
    // When using `ui::TestClipboard` (via
    // `content::BrowserTestClipboardScope`), clipboard reads/writes are
    // synchronous. If we poll the clipboard synchronously during construction,
    // the state change will be triggered before Kombucha has finished
    // registering this observer, causing it to miss the notification. Deferring
    // the poll via `PostTask` ensures the observer is fully registered and
    // ready to receive the state change.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ClipboardTextObserver::PollClipboardImpl,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void PollClipboardImpl() {
    ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste,
                        /*data_dst=*/std::nullopt,
                        base::BindOnce(&ClipboardTextObserver::GotClipboard,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GotClipboard(std::u16string result) {
    OnStateObserverStateChanged(std::move(result));
  }

  base::WeakPtrFactory<ClipboardTextObserver> weak_ptr_factory_{this};
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ClipboardTextObserver, kClipboardText);

}  // namespace

namespace contextual_tasks {

class ContextualTasksInteractiveUiTestBase
    : public ContextualTasksInteractiveTestBase {
 public:
  ContextualTasksInteractiveUiTestBase() = default;
  ~ContextualTasksInteractiveUiTestBase() override = default;

  void SetUpOnMainThread() override {
    ContextualTasksInteractiveTestBase::SetUpOnMainThread();

    // Explicitly enable user-level content sharing settings to satisfy native
    // FeatureEligibility.
    browser()->GetProfile()->GetPrefs()->SetInteger(
        contextual_search::kSearchContentSharingSettings,
        static_cast<int>(
            contextual_search::SearchContentSharingSettingsValue::kEnabled));

    // Disable side panel animations to avoid WaitForShow/WaitForHide flakiness.
    SidePanelUI::From(browser())->DisableAnimationsForTesting();
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    ContextualTasksInteractiveTestBase::TearDownOnMainThread();
  }

  static auto WaitForElementExists(const ui::ElementIdentifier& contents_id,
                                   const DeepQuery& element) {
    StateChange change;
    change.type = StateChange::Type::kExists;
    change.where = element;
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  static auto WaitForElementDoesNotExist(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange change;
    change.type = StateChange::Type::kDoesNotExist;
    change.where = element;
    change.event = kElementDoesNotExistEvent;
    return WaitForStateChange(contents_id, change);
  }

  static auto WaitForElementVisible(const ui::ElementIdentifier& contents_id,
                                    const DeepQuery& element) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = element;
    change.test_function =
        "(el) => { const rect = el.getBoundingClientRect(); "
        "const left = Math.max(0, rect.x); "
        "const top = Math.max(0, rect.y); "
        "const right = Math.min(rect.x + rect.width, window.innerWidth); "
        "const bottom = Math.min(rect.y + rect.height, window.innerHeight); "
        "return right > left && bottom > top; }";
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  static auto ClickButton(const ui::ElementIdentifier& contents_id,
                          const DeepQuery& element) {
    return ExecuteJsAt(contents_id, element, "el => el.click()");
  }

  auto ForceClickAddContextEntrypoint(
      const ui::ElementIdentifier& contents_id) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = R"(
        function(app) {
          const composebox = app?.shadowRoot
              ?.querySelector('#composebox')
              ?.shadowRoot
              ?.querySelector('#composebox');
          if (!composebox) return false;
          const contextEntrypoint = composebox.shadowRoot
              ?.querySelector('#contextEntrypoint');
          const menu = contextEntrypoint?.shadowRoot
              ?.querySelector('#menu');
          if (composebox.contextMenuOpened && menu?.open &&
              contextEntrypoint?.tabSuggestionsState !== 1) {
            if (menu.tabSuggestions && menu.tabSuggestions.length > 0) {
              if (!menu.shadowRoot?.querySelector(
                      'button.dropdown-item[data-index="0"]')) {
                return false;
              }
            } else if (!menu.shadowRoot?.querySelector(
                           'button.dropdown-item')) {
              return false;
            }
            app._entrypointClicked = false;
            app._entrypointClickTime = 0;
            return true;
          }
          const now = Date.now();
          if (app._entrypointClicked &&
              (!composebox.contextMenuOpened || !menu || !menu.open)) {
            if (now - (app._entrypointClickTime || 0) > 1500) {
              app._entrypointClicked = false;
            }
          }
          if (!app._entrypointClicked) {
            const btn = contextEntrypoint?.shadowRoot
                ?.querySelector('#entrypointButton')
                ?.shadowRoot
                ?.querySelector('#entrypoint');
            if (btn && !btn.disabled) {
              app._entrypointClicked = true;
              app._entrypointClickTime = now;
              btn.click();
            }
          }
          return false;
        }
        )";
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  // Helper to force click a button in the context menu matching the selector.
  auto ForceClickMenuButtonBySelector(const ui::ElementIdentifier& contents_id,
                                      const std::string& selector) {
    static int s_click_id = 0;
    int click_id = ++s_click_id;
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        R"(
        function(app) {
          const composebox = app?.shadowRoot
              ?.querySelector('#composebox')
              ?.shadowRoot
              ?.querySelector('#composebox');
          if (!composebox) return false;
          const contextEntrypoint = composebox.shadowRoot
              ?.querySelector('#contextEntrypoint');
          const menu = contextEntrypoint?.shadowRoot
              ?.querySelector('#menu');
          if (app._lastMenuClickId === %d) {
            if (!composebox.contextMenuOpened || !menu || !menu.open) {
              return true;
            }
            const btn = menu.shadowRoot?.querySelector('%s');
            if (btn &&
                btn.getAttribute('aria-checked') !== app._lastMenuClickAria) {
              return true;
            }
            const now = Date.now();
            if (now - (app._lastMenuClickTime || 0) > 1500) {
              app._lastMenuClickId = 0;
            }
            return false;
          }
          if (!composebox.contextMenuOpened || !menu || !menu.open) {
            const now = Date.now();
            if (!app._menuReopenTime || now - app._menuReopenTime > 1500) {
              app._menuReopenTime = now;
              const entryBtn = contextEntrypoint?.shadowRoot
                  ?.querySelector('#entrypointButton')?.shadowRoot
                  ?.querySelector('#entrypoint');
              if (entryBtn && !entryBtn.disabled) {
                entryBtn.click();
              }
            }
            return false;
          }
          app._menuReopenTime = 0;
          const btn = menu.shadowRoot?.querySelector('%s');
          if (btn && !btn.disabled) {
            app._lastMenuClickId = %d;
            app._lastMenuClickTime = Date.now();
            app._lastMenuClickAria = btn.getAttribute('aria-checked');
            btn.click();
            return false;
          }
          return false;
        }
        )",
        click_id, selector.c_str(), selector.c_str(), click_id);
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  // Forces a click on a context menu item identified by its data-index.
  // This is used when the menu item does not have a specific ID but can be
  // identified by its position or index in the list.
  auto ForceClickMenuButton(const ui::ElementIdentifier& contents_id,
                            int target_index) {
    return ForceClickMenuButtonBySelector(
        contents_id,
        base::StringPrintf("button.dropdown-item[data-index=\"%d\"]",
                           target_index));
  }

  // Forces a click on a context menu item identified by its ID string.
  auto ForceClickMenuButton(const ui::ElementIdentifier& contents_id,
                            const std::string& button_id) {
    return ForceClickMenuButtonBySelector(contents_id, "#" + button_id);
  }

  auto WaitForMenuTabSelected(const ui::ElementIdentifier& contents_id,
                              int target_index) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        R"(
        function(app) {
          const composebox = app?.shadowRoot
              ?.querySelector('#composebox')
              ?.shadowRoot
              ?.querySelector('#composebox');
          if (!composebox) return false;
          const contextEntrypoint = composebox.shadowRoot
              ?.querySelector('#contextEntrypoint');
          const menu = contextEntrypoint?.shadowRoot
              ?.querySelector('#menu');
          if (!composebox.contextMenuOpened || !menu || !menu.open) {
            const now = Date.now();
            if (!app._menuReopenTime || now - app._menuReopenTime > 1500) {
              app._menuReopenTime = now;
              const entryBtn = contextEntrypoint?.shadowRoot
                  ?.querySelector('#entrypointButton')?.shadowRoot
                  ?.querySelector('#entrypoint');
              if (entryBtn && !entryBtn.disabled) {
                entryBtn.click();
              }
            }
            return false;
          }
          app._menuReopenTime = 0;
          const btn = menu.shadowRoot
              ?.querySelector('button.dropdown-item[data-index="%d"]');
          return !!(btn && !btn.disabled &&
                    btn.getAttribute('aria-checked') === 'true');
        }
        )",
        target_index);
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto WaitForComposeboxFilesCount(const ui::ElementIdentifier& contents_id,
                                   int expected_count) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app", "#composebox", "#composebox"};
    change.test_function = base::StringPrintf(
        "el => el.attachedContext && el.attachedContext.size === %d",
        expected_count);
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto WaitForComposeboxFilesCount(int expected_count) {
    return WaitForComposeboxFilesCount(kPrimaryTab, expected_count);
  }

  // Waits until the composebox holds `expected_count` attachments and every
  // attachment upload has reached a terminal status. Interacting with a chip
  // (e.g. removing it) while its upload is still in flight races with the
  // asynchronous upload status updates on slow bots.
  auto WaitForComposeboxFileUploadsComplete(
      const ui::ElementIdentifier& contents_id,
      int expected_count) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app", "#composebox", "#composebox"};
    change.test_function = base::StringPrintf(
        "el => !!el.attachedContext && el.attachedContext.size === %d && "
        "el.fileUploadsComplete === true",
        expected_count);
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto WaitForFaviconGroupWithTitle(const ui::ElementIdentifier& contents_id,
                                    const std::string& expected_title) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        "function(app) {"
        "  const el = "
        "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
        "querySelector('#composebox')?.shadowRoot?."
        "querySelector('#contextEntrypoint')?.shadowRoot?."
        "querySelector('#entrypointButton')?.shadowRoot?."
        "querySelector('composebox-favicon-group');"
        "  if (el && el.tabs) {"
        "    return el.tabs.some(t => t.title.includes('%s'));"
        "  }"
        "  return false;"
        "}",
        expected_title.c_str());
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto WaitForDocumentChipWithTitle(const ui::ElementIdentifier& contents_id,
                                    const std::string& expected_title) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        "function(app) {"
        "  const el = "
        "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
        "querySelector('#composebox')?.shadowRoot?."
        "querySelector('#carousel')?.shadowRoot?."
        "querySelector('cr-composebox-file-thumbnail')?.shadowRoot?."
        "querySelector('#documentChip')?.querySelector('#documentTitle');"
        "  if (el && el.textContent.trim().includes('%s')) {"
        "    return true;"
        "  }"
        "  return false;"
        "}",
        expected_title.c_str());
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto WaitForTabChipWithTitle(const ui::ElementIdentifier& contents_id,
                               const std::string& expected_title) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app"};
    change.test_function = base::StringPrintf(
        "function(app) {"
        "  const el = "
        "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
        "querySelector('#composebox')?.shadowRoot?."
        "querySelector('#carousel')?.shadowRoot?."
        "querySelector('cr-composebox-file-thumbnail')?.shadowRoot?."
        "querySelector('#tabChip')?.querySelector('div.tabInfo > div.title');"
        "  if (el && el.textContent.trim().includes('%s')) {"
        "    return true;"
        "  }"
        "  return false;"
        "}",
        expected_title.c_str());
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto WaitForInterceptionAndLoad() {
    return Steps(WaitForWebContentsNavigation(kPrimaryTab),
                 CheckElement(kPrimaryTab,
                              [](ui::TrackedElement* el) {
                                return AsInstrumentedWebContents(el)
                                           ->web_contents()
                                           ->GetLastCommittedURL()
                                           .host() ==
                                       chrome::kChromeUIContextualTasksHost;
                              }),
                 WaitForElementExists(kPrimaryTab, {"contextual-tasks-app"}));
  }

  auto OpenContextualTasksInCurrentTab(const GURL& interception_url) {
    return Steps(Do(base::BindLambdaForTesting([this, interception_url]() {
                   browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetController()
                       .LoadURL(interception_url, content::Referrer(),
                                ui::PAGE_TRANSITION_TYPED, std::string());
                 })),
                 WaitForInterceptionAndLoad());
  }

  auto OpenContextualTasksInSidePanel(
      const ui::ElementIdentifier& side_panel_id) {
    return Steps(
        Do(base::BindLambdaForTesting([this]() {
          SidePanelUI::From(browser())->DisableAnimationsForTesting();
          ContextualTasksPanelController::From(browser())->Show(
              false,
              omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
        })),
        WaitForShow(kContextualTasksSidePanelWebViewElementId),
        NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                         "SidePanelContentWebViewName",
                         [](ContextualTasksWebView* web_view) -> views::View* {
                           return web_view->content_web_view();
                         }),
        InstrumentNonTabWebView(side_panel_id, "SidePanelContentWebViewName"),
        WaitForElementExists(side_panel_id, {"contextual-tasks-app",
                                             "#composebox", "#composebox"}));
  }

  // Verifies the structure and content of the SubmitQuery protobuf message sent
  // from the browser to the inner WebContents.
  MultiStep VerifySubmitQueryMessage(
      lens::LensOverlayRequestId::MediaType expected_media_type,
      std::optional<std::string> expected_added_input_name = std::nullopt,
      int expected_message_index = 0,
      std::optional<lens::LensOverlayContextualInputUploadType>
          expected_upload_type = std::nullopt) {
    return VerifySubmitQueryMessageImpl(
        expected_media_type, expected_added_input_name, expected_message_index,
        expected_upload_type, nullptr, nullptr);
  }

  MultiStep VerifySubmitQueryMessageAndExtractUuid(
      lens::LensOverlayRequestId::MediaType expected_media_type,
      std::optional<std::string> expected_added_input_name,
      int expected_message_index,
      std::optional<lens::LensOverlayContextualInputUploadType>
          expected_upload_type,
      std::reference_wrapper<uint64_t> out_uuid) {
    return VerifySubmitQueryMessageImpl(
        expected_media_type, expected_added_input_name, expected_message_index,
        expected_upload_type, nullptr, &out_uuid.get());
  }

  MultiStep VerifySubmitQueryMessageWithExpectedUuid(
      lens::LensOverlayRequestId::MediaType expected_media_type,
      std::optional<std::string> expected_added_input_name,
      int expected_message_index,
      std::optional<lens::LensOverlayContextualInputUploadType>
          expected_upload_type,
      std::reference_wrapper<uint64_t> expected_uuid) {
    return VerifySubmitQueryMessageImpl(
        expected_media_type, expected_added_input_name, expected_message_index,
        expected_upload_type, &expected_uuid.get(), nullptr);
  }

 public:
  MultiStep VerifySubmitQueryMessageImpl(
      lens::LensOverlayRequestId::MediaType expected_media_type,
      std::optional<std::string> expected_added_input_name,
      int expected_message_index,
      std::optional<lens::LensOverlayContextualInputUploadType>
          expected_upload_type,
      const uint64_t* expected_uuid,
      uint64_t* out_uuid) {
    return Steps(
        // Wait until the inner WebContents receives the message at the expected
        // index.
        WaitForJsResult(
            kInnerWebContentsId,
            base::StringPrintf(
                "() => Boolean(Array.isArray(window.receivedMessages) && "
                "window.receivedMessages.filter(buf => new "
                "Uint8Array(buf)[0] === 18).length > %d)",
                expected_message_index)),
        WithElement(kInnerWebContentsId, [expected_media_type,
                                          expected_added_input_name,
                                          expected_message_index,
                                          expected_upload_type, expected_uuid,
                                          out_uuid](ui::TrackedElement* el) {
          auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
          // Extract the binary protobuf message from JavaScript by converting
          // the matching ArrayBuffer into a base64 encoded string.
          std::string base64_msg =
              content::EvalJs(
                  web_contents,
                  base::StringPrintf(
                      "btoa(Array.from(new "
                      "Uint8Array(window.receivedMessages.filter(buf => new "
                      "Uint8Array(buf)[0] === 18)[%d])).map(b => "
                      "String.fromCharCode(b)).join(''))",
                      expected_message_index))
                  .ExtractString();
          std::string decoded_msg;
          ASSERT_TRUE(base::Base64Decode(base64_msg, &decoded_msg));
          lens::ClientToAimMessage message;
          ASSERT_TRUE(message.ParseFromString(decoded_msg));

          // Verify core query payload requirements.
          EXPECT_TRUE(message.has_submit_query());
          ASSERT_EQ(
              message.submit_query().payload().lens_image_query_data_size(), 1);
          EXPECT_EQ(message.submit_query()
                        .payload()
                        .lens_image_query_data(0)
                        .request_id()
                        .media_type(),
                    expected_media_type);

          uint64_t current_uuid = message.submit_query()
                                      .payload()
                                      .lens_image_query_data(0)
                                      .request_id()
                                      .uuid();
          if (expected_uuid) {
            EXPECT_EQ(current_uuid, *expected_uuid);
          }
          if (out_uuid) {
            *out_uuid = current_uuid;
          }

          // Verify additional context inputs if expected by the test case.
          if (expected_added_input_name.has_value()) {
            EXPECT_TRUE(message.submit_query().payload().has_added_inputs());
            EXPECT_EQ(message.submit_query()
                          .payload()
                          .added_inputs()
                          .added_inputs_size(),
                      1);
            EXPECT_TRUE(message.submit_query()
                            .payload()
                            .added_inputs()
                            .added_inputs(0)
                            .has_lens_file());
            if (expected_media_type ==
                lens::LensOverlayRequestId::MEDIA_TYPE_PDF) {
              EXPECT_THAT(message.submit_query()
                              .payload()
                              .added_inputs()
                              .added_inputs(0)
                              .lens_file()
                              .file_name(),
                          testing::HasSubstr(*expected_added_input_name));
            } else {
              EXPECT_THAT(message.submit_query()
                              .payload()
                              .added_inputs()
                              .added_inputs(0)
                              .lens_file()
                              .page_url(),
                          testing::HasSubstr(*expected_added_input_name));
            }
          } else {
            EXPECT_FALSE(message.submit_query().payload().has_added_inputs());
          }

          if (expected_upload_type.has_value()) {
            bool found_type = false;
            const auto& data_list =
                message.submit_query().payload().lens_image_query_data();
            for (const auto& payload : data_list) {
              if (payload.contextual_input_upload_type() ==
                  *expected_upload_type) {
                found_type = true;
                break;
              }
            }
            EXPECT_TRUE(found_type)
                << "Expected upload type not found in lens_image_query_data.";
          }
        }));
  }

 public:
  MultiStep VerifyMultipleSubmitQueryMessage(
      const std::string& expected_query_text,
      int expected_viewport_image_count,
      int expected_upload_image_count,
      int expected_upload_file_count,
      std::vector<std::string> expected_added_input_names,
      int expected_message_index = 0) {
    return VerifyMultipleSubmitQueryMessageImpl(
        expected_query_text, expected_viewport_image_count,
        expected_upload_image_count, expected_upload_file_count,
        expected_added_input_names, expected_message_index, nullptr, nullptr);
  }

  MultiStep VerifyMultipleSubmitQueryMessageAndExtractUuid(
      const std::string& expected_query_text,
      int expected_viewport_image_count,
      int expected_upload_image_count,
      int expected_upload_file_count,
      std::vector<std::string> expected_added_input_names,
      int expected_message_index,
      std::reference_wrapper<uint64_t> out_uuid) {
    return VerifyMultipleSubmitQueryMessageImpl(
        expected_query_text, expected_viewport_image_count,
        expected_upload_image_count, expected_upload_file_count,
        expected_added_input_names, expected_message_index, nullptr,
        &out_uuid.get());
  }

  MultiStep VerifyMultipleSubmitQueryMessageWithExpectedUuid(
      const std::string& expected_query_text,
      int expected_viewport_image_count,
      int expected_upload_image_count,
      int expected_upload_file_count,
      std::vector<std::string> expected_added_input_names,
      int expected_message_index,
      std::reference_wrapper<uint64_t> expected_uuid) {
    return VerifyMultipleSubmitQueryMessageImpl(
        expected_query_text, expected_viewport_image_count,
        expected_upload_image_count, expected_upload_file_count,
        expected_added_input_names, expected_message_index,
        &expected_uuid.get(), nullptr);
  }

 public:
  MultiStep VerifyMultipleSubmitQueryMessageImpl(
      const std::string& expected_query_text,
      int expected_viewport_image_count,
      int expected_upload_image_count,
      int expected_upload_file_count,
      std::vector<std::string> expected_added_input_names,
      int expected_message_index,
      const uint64_t* expected_uuid,
      uint64_t* out_uuid) {
    return Steps(
        // Wait until the inner WebContents receives the message at the expected
        // index.
        WaitForJsResult(
            kInnerWebContentsId,
            base::StringPrintf(
                "() => Boolean(Array.isArray(window.receivedMessages) && "
                "window.receivedMessages.filter(buf => new "
                "Uint8Array(buf)[0] === 18).length > %d)",
                expected_message_index)),
        WithElement(kInnerWebContentsId, [expected_query_text,
                                          expected_viewport_image_count,
                                          expected_upload_image_count,
                                          expected_upload_file_count,
                                          expected_added_input_names,
                                          expected_message_index, expected_uuid,
                                          out_uuid](ui::TrackedElement* el) {
          auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
          // Extract the binary protobuf message from JavaScript by converting
          // the matching ArrayBuffer into a base64 encoded string.
          std::string base64_msg =
              content::EvalJs(
                  web_contents,
                  base::StringPrintf(
                      "btoa(Array.from(new "
                      "Uint8Array(window.receivedMessages.filter(buf => new "
                      "Uint8Array(buf)[0] === 18)[%d])).map(b => "
                      "String.fromCharCode(b)).join(''))",
                      expected_message_index))
                  .ExtractString();
          std::string decoded_msg;
          ASSERT_TRUE(base::Base64Decode(base64_msg, &decoded_msg));
          lens::ClientToAimMessage message;
          ASSERT_TRUE(message.ParseFromString(decoded_msg));

          // Verify core query payload requirements.
          EXPECT_TRUE(message.has_submit_query());
          EXPECT_EQ(message.submit_query().payload().query_text(),
                    expected_query_text);
          EXPECT_EQ(
              message.submit_query().payload().lens_image_query_data_size(),
              expected_viewport_image_count + expected_upload_image_count +
                  expected_upload_file_count);

          if (message.submit_query().payload().lens_image_query_data_size() >
              0) {
            uint64_t current_uuid = message.submit_query()
                                        .payload()
                                        .lens_image_query_data(0)
                                        .request_id()
                                        .uuid();
            if (expected_uuid) {
              EXPECT_EQ(current_uuid, *expected_uuid);
            }
            if (out_uuid) {
              *out_uuid = current_uuid;
            }
          }

          // Verify additional context inputs.
          if (!expected_added_input_names.empty()) {
            EXPECT_TRUE(message.submit_query().payload().has_added_inputs());
            auto added_inputs = message.submit_query().payload().added_inputs();
            EXPECT_EQ(added_inputs.added_inputs_size(),
                      static_cast<int>(expected_added_input_names.size()));

            for (const auto& expected_name : expected_added_input_names) {
              bool found = false;
              for (int i = 0; i < added_inputs.added_inputs_size(); ++i) {
                const auto& input = added_inputs.added_inputs(i);
                if (input.has_lens_file()) {
                  std::string name = input.lens_file().file_name();
                  if (name.empty()) {
                    name = input.lens_file().page_url();
                  }
                  if (name.find(expected_name) != std::string::npos) {
                    found = true;
                    break;
                  }
                }
              }
              EXPECT_TRUE(found)
                  << "Expected added input not found: " << expected_name;
            }
          } else {
            EXPECT_FALSE(message.submit_query().payload().has_added_inputs());
          }
        }));
  }

  // Inject text via web events on the composebox input, after waiting for the
  // composebox first update + searchbox inputState init to settle.
  auto InputText(ui::ElementIdentifier contents_id,
                 std::string_view query_text) {
    const DeepQuery kComposeboxHost = {"contextual-tasks-app", "#composebox",
                                       "#composebox"};
    const DeepQuery kComposeboxInput = {"contextual-tasks-app", "#composebox",
                                        "#composebox", "#composeboxInput",
                                        "#input"};
    return Steps(
        WaitForElementExists(contents_id, kComposeboxHost),
        WaitForJsResultAt(contents_id, kComposeboxHost,
                          "el => el.hasUpdated", true),
        WaitForJsResultAt(
            contents_id, kComposeboxHost,
            "el => el.getSearchboxHandler().getInputState().then(() => true)",
            true),
        ClickElement(contents_id, kComposeboxInput),
        ExecuteJsAt(
            contents_id, kComposeboxInput,
            content::JsReplace(
                "(el) => { "
                "  el.value = $1; "
                "  el.dispatchEvent(new Event('input', { bubbles: true })); "
                "  el.dispatchEvent(new Event('change', { bubbles: true })); "
                "}",
                query_text)));
  }

  auto CloseContextualTasksSidePanel() {
    return Steps(
        Do(base::BindLambdaForTesting([this]() {
          contextual_tasks::ContextualTasksPanelController::From(browser())
              ->Close();
        })),
        WaitForHide(kContextualTasksSidePanelWebViewElementId));
  }

  auto WaitForInputValue(ui::ElementIdentifier contents_id,
                         std::string_view expected,
                         bool continue_across_navigation = false) {
    const WebContentsInteractionTestUtil::DeepQuery kComposeboxHostPath = {
        "contextual-tasks-app", "#composebox", "#composebox"};
    return WaitForJsResultAt(contents_id, kComposeboxHostPath,
                             "el => el.input", std::string(expected),
                             /*element_must_be_present_at_start=*/false,
                             continue_across_navigation);
  }

  auto WaitForInputCleared(ui::ElementIdentifier contents_id,
                           bool continue_across_navigation = false) {
    return WaitForInputValue(contents_id, "", continue_across_navigation);
  }

  // cr-composebox-submit reflects `disabled` to its host.
  auto WaitForSubmitButtonEnabled(ui::ElementIdentifier contents_id) {
    const WebContentsInteractionTestUtil::DeepQuery kSubmitButtonHostPath = {
        "contextual-tasks-app", "#composebox", "#composebox",
        "cr-composebox-submit"};
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = kSubmitButtonHostPath;
    change.test_function = "(el) => !el.disabled";
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  auto SubmitQueryViaSubmitButton(ui::ElementIdentifier contents_id) {
    const WebContentsInteractionTestUtil::DeepQuery kSubmitButtonHostPath = {
        "contextual-tasks-app", "#composebox", "#composebox",
        "cr-composebox-submit"};
    const WebContentsInteractionTestUtil::DeepQuery kSubmitButtonPath = {
        "contextual-tasks-app", "#composebox", "#composebox",
        "cr-composebox-submit", "#submitContainer"};
    return Steps(WaitForElementExists(contents_id, kSubmitButtonHostPath),
                 WaitForSubmitButtonEnabled(contents_id),
                 ClickButton(contents_id, kSubmitButtonPath));
  }

  auto ClickNewThreadButton(ui::ElementIdentifier side_panel_id) {
    const WebContentsInteractionTestUtil::DeepQuery kNewThreadButtonPath = {
        "contextual-tasks-app", "#toolbar", "#newThreadButton"};
    // The click navigates the embedded thread frame, so the click step must not
    // wait for a completion response that the navigation would drop.
    return Steps(WaitForElementExists(side_panel_id, kNewThreadButtonPath),
                 ExecuteJsAt(side_panel_id, kNewThreadButtonPath,
                             "el => el.click()",
                             ExecuteJsMode::kFireAndForget));
  }

  // shouldShowVoiceSearch() requires 'webkitSpeechRecognition' in window.
  auto EnsureVoiceSearchAvailable(ui::ElementIdentifier contents_id) {
    const WebContentsInteractionTestUtil::DeepQuery kComposeboxHostPath = {
        "contextual-tasks-app", "#composebox", "#composebox"};
    return ExecuteJsAt(
        contents_id, kComposeboxHostPath,
        "(el) => { "
        "  if (!('webkitSpeechRecognition' in window)) {"
        "    Object.defineProperty(window, 'webkitSpeechRecognition', { "
        "      configurable: true,"
        "      value: class { start() {} stop() {} abort() {} }, "
        "    }); "
        "  } "
        "  el.requestUpdate(); "
        "}");
  }

  auto DispatchVoiceSearchFinalResult(ui::ElementIdentifier contents_id,
                                      std::string_view transcript) {
    const WebContentsInteractionTestUtil::DeepQuery kVoiceSearchComponentPath =
        {"contextual-tasks-app", "#composebox", "#composebox", "#voiceSearch"};
    return ExecuteJsAt(
        contents_id, kVoiceSearchComponentPath,
        content::JsReplace("(el) => el.dispatchEvent(new CustomEvent("
                           "'voice-search-final-result', "
                           "{ detail: $1, bubbles: true, composed: true}))",
                           transcript));
  }

 private:
  gfx::ScopedAnimationDurationScaleMode disable_animations_{
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION};
};

// TestLensSearchController that can use the real LensQueryFlowRouter (and real
// ContextualSearchSessionHandle / ComposeboxQueryController) instead of
// FakeLensQueryFlowRouter.
class ConfigurableQueryRouterTestLensSearchController
    : public lens::TestLensSearchController {
 public:
  ConfigurableQueryRouterTestLensSearchController(
      tabs::TabInterface* tab,
      const bool* use_real_query_flow_router)
      : lens::TestLensSearchController(tab),
        use_real_query_flow_router_(use_real_query_flow_router) {}

  std::unique_ptr<lens::LensQueryFlowRouter> CreateLensQueryFlowRouter()
      override {
    if (*use_real_query_flow_router_) {
      return LensSearchController::CreateLensQueryFlowRouter();
    }
    auto router = lens::TestLensSearchController::CreateLensQueryFlowRouter();
    auto* fake_router =
        static_cast<lens::FakeLensQueryFlowRouter*>(router.get());
    auto* session_handle =
        static_cast<contextual_search::MockContextualSearchSessionHandle*>(
            fake_router->GetContextualSearchSessionHandle());
    ON_CALL(*session_handle, CreateSearchUrl(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::WithArg<1>([](base::OnceCallback<void(GURL)> callback) {
              std::move(callback).Run(
                  GURL("https://www.google.com/search?lns_mode=un&vsrid=123"));
            }));
    return router;
  }

 private:
  raw_ptr<const bool> use_real_query_flow_router_;
};

class ContextualTasksInteractiveUiTest
    : public ContextualTasksInteractiveUiTestBase,
      public testing::WithParamInterface<UserVariation> {
 public:
  ContextualTasksInteractiveUiTest() {
    lens_search_controller_override_ = ui::UserDataFactory::ScopedOverride();
    lens_search_controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating(
                [](const bool* use_real_query_flow_router,
                   tabs::TabInterface& tab) {
                  return std::make_unique<
                      ConfigurableQueryRouterTestLensSearchController>(
                      &tab, use_real_query_flow_router);
                },
                &use_real_lens_query_flow_router_));
  }
  ~ContextualTasksInteractiveUiTest() override = default;

  void UseRealLensQueryFlowRouter() { use_real_lens_query_flow_router_ = true; }

  UserVariation GetUserVariation() const override { return GetParam(); }

 private:
  bool use_real_lens_query_flow_router_ = false;
};

class ContextualTasksPinnedToolbarInteractiveUiTest
    : public ContextualTasksInteractiveUiTest {
 public:
  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    auto disabled = GetDefaultDisabledFeatures();
    enabled.push_back({kEnableContextualTasksPinButtonInToolbar, {}});
    enabled.push_back({kContextualTasksForceEntryPointEligibility, {}});
    // `RestoreClosedTabAndReopenPanel` expects a reopened tab to come back
    // without its side panel.
    disabled.push_back(kContextualTasksSidePanelTabState);
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }
};

// This tests the following CUJ:
//  (1) User opens Customize Chrome toolbar section to pin Contextual Tasks.
//  (2) User toggles Contextual Tasks pinning via Customize Chrome WebUI.
//  (3) Pinned Contextual Tasks toolbar button appears.
//  (4) User clicks the pinned Contextual Tasks toolbar button.
//  (5) Contextual Tasks side panel opens and WebUI app initializes.
//  (6) Pinned toolbar button reflects the active toggle state.
//  (7) User clicks the pinned toolbar button again to toggle the side panel
//      closed.
//  (8) Contextual Tasks side panel closes.
IN_PROC_BROWSER_TEST_P(ContextualTasksPinnedToolbarInteractiveUiTest,
                       PinnedToolbarButtonClick) {
  SkipIfIncognito("Toolbar pinning is not supported in Incognito");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCustomizeChromeWebContentsId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kContextualTasksToggle = {
      "customize-chrome-app",
      "#toolbarPage",
      base::StrCat({"cr-toggle[aria-label=\"",
                    l10n_util::GetStringUTF8(
                        IDS_CONTEXTUAL_TASKS_CUSTOMIZE_CHROME_LABEL),
                    "\"]"}),
  };

  RunTestSequence(
      // Step 1: Open Customize Chrome toolbar section to pin Contextual Tasks.
      Do([this]() {
        chrome::ExecuteCommand(browser(), IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR);
      }),
      WaitForShow(kCustomizeChromeSidePanelWebViewElementId),
      InstrumentNonTabWebView(kCustomizeChromeWebContentsId,
                              kCustomizeChromeSidePanelWebViewElementId),
      // Step 2: Toggle pinning for Contextual Tasks via Customize Chrome WebUI.
      WaitForJsResultAt(
          kCustomizeChromeWebContentsId, kContextualTasksToggle,
          "(el) => { const r = el.getBoundingClientRect(); return r.width > 0 "
          "&& r.height > 0; }"),
      ScrollIntoView(kCustomizeChromeWebContentsId, kContextualTasksToggle),
      WaitForElementVisible(kCustomizeChromeWebContentsId,
                            kContextualTasksToggle),
      ClickElement(kCustomizeChromeWebContentsId, kContextualTasksToggle),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),

      // Step 3 & 4: Click the pinned button to open the side panel and verify
      // WebUI loads.
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      CheckResult(
          [this]() {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          true),

      // Step 5: Verify the pinned toolbar button reflects the active toggle
      // state.
      CheckResult(
          [this]() {
            return PinnedActionTestAccessor(browser(),
                                            kActionSidePanelShowContextualTasks)
                .IsHighlighted();
          },
          true),

      // Step 6: Instrument WebUI app inside side panel to verify load.
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      WaitForElementExists(kSidePanelWebContentsId, {"contextual-tasks-app"}),

      // Step 7: Click the pinned button again to toggle the side panel closed.
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForHide(kContextualTasksSidePanelWebViewElementId),
      CheckResult(
          [this]() {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          false),

      // Step 8: Verify the pinned toolbar button is no longer active.
      CheckResult(
          [this]() {
            return PinnedActionTestAccessor(browser(),
                                            kActionSidePanelShowContextualTasks)
                .IsHighlighted();
          },
          false));
}

// This tests the following CUJ:
//  (1) User has a webpage open in a window with a second tab.
//  (2) User clicks the pinned Contextual Tasks toolbar button.
//  (3) Contextual Tasks side panel opens with a task for the active tab.
//  (4) User types a draft query into the composebox without submitting it.
//  (5) User switches to the second tab. The side panel closes because that
//      tab has no task.
//  (6) User switches back to the first tab. The side panel reopens for the
//      same task.
//  (7) The unsubmitted draft query is still in the composebox.
IN_PROC_BROWSER_TEST_P(ContextualTasksPinnedToolbarInteractiveUiTest,
                       SwitchTabsPreservesComposeboxDraft) {
  SkipIfIncognito("Toolbar pinning is not supported in Incognito");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReopenedSidePanelId);

  const GURL kPageUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kOtherPageUrl = embedded_test_server()->GetURL("/title2.html");
  const DeepQuery kComposebox = {"contextual-tasks-app", "#composebox",
                                 "#composebox"};
  constexpr char kDraftQuery[] = "Draft follow-up question";

  PinnedToolbarActionsModel::Get(browser()->GetProfile())
      ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);

  // The side panel WebContents for the task, recorded when the panel first
  // opens.
  content::WebContents* panel_contents = nullptr;

  auto is_panel_open = [this]() {
    return ContextualTasksPanelController::From(browser())
        ->IsPanelOpenForContextualTask();
  };

  RunTestSequence(
      // (1) Open a webpage in the first tab, with a second tab in the window.
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kPageUrl),
      AddInstrumentedTab(kGenericTab, kOtherPageUrl),
      WaitForWebContentsReady(kGenericTab, kOtherPageUrl),
      SelectTab(kTabStripElementId, 0),

      // (2) Click the pinned Contextual Tasks toolbar button.
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),

      // (3) The side panel opens with a task for the active tab.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "DraftSidePanelContents",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelId, "DraftSidePanelContents"),
      WaitForElementExists(kSidePanelId, kComposebox),
      // Wait for the thread frame to load. Zero-state init clears the
      // composebox input before it loads the thread frame, so text typed
      // earlier would be lost.
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),
      Do([&]() {
        panel_contents = ContextualTasksPanelController::From(browser())
                             ->GetActiveWebContents();
      }),
      Check([&]() { return panel_contents != nullptr; },
            "Side panel shows the task's WebContents"),

      // (4) Type a draft query without submitting it.
      InputText(kSidePanelId, kDraftQuery),
      WaitForInputValue(kSidePanelId, kDraftQuery),
      // Stop tracking the side panel contents before the panel hides.
      UninstrumentWebContents(kInnerWebContentsId),
      UninstrumentWebContents(kSidePanelId),

      // (5) Switch to the second tab. It has no task, so the side panel
      // closes.
      SelectTab(kTabStripElementId, 1),
      WaitForHide(kContextualTasksSidePanelWebViewElementId),
      CheckResult(is_panel_open, false),

      // (6) Switch back to the first tab. The side panel reopens for the
      // same task, reusing its cached WebContents.
      SelectTab(kTabStripElementId, 0),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      CheckResult(is_panel_open, true),
      Check(
          [&]() {
            return ContextualTasksPanelController::From(browser())
                       ->GetActiveWebContents() == panel_contents;
          },
          "Side panel shows the same task WebContents"),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "DraftReopenedSidePanelContents",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kReopenedSidePanelId,
                              "DraftReopenedSidePanelContents"),

      // (7) The unsubmitted draft query is still in the composebox.
      WaitForInputValue(kReopenedSidePanelId, kDraftQuery));
}

// This tests the following CUJ:
//  (1) User has a webpage open in a window with a second tab.
//  (2) User clicks the pinned Contextual Tasks toolbar button.
//  (3) Contextual Tasks side panel opens with a task for the active tab.
//  (4) User moves the active tab to a new window.
//  (5) The tab opens in the new window and keeps its task. The side panel in
//      the original window closes and does not open in the new window.
//  (6) User clicks the pinned Contextual Tasks toolbar button in the new
//      window.
//  (7) Contextual Tasks side panel opens in the new window with a new
//      zero-state task for the moved tab.
IN_PROC_BROWSER_TEST_P(ContextualTasksPinnedToolbarInteractiveUiTest,
                       MoveTabToNewWindowAndReopenPanel) {
  SkipIfIncognito("Toolbar pinning is not supported in Incognito");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMovedTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewWindowSidePanelId);

  const GURL kPageUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kOtherPageUrl = embedded_test_server()->GetURL("/title2.html");
  const DeepQuery kComposebox = {"contextual-tasks-app", "#composebox",
                                 "#composebox"};

  // Pinning is profile-wide, so the pinned button also shows in the window
  // that the tab moves to.
  PinnedToolbarActionsModel::Get(browser()->GetProfile())
      ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);

  // The tab's WebContents stays alive when the tab moves between windows.
  content::WebContents* moved_contents = nullptr;
  std::optional<base::Uuid> task_id;

  auto get_task_id = [this, &moved_contents]() -> std::optional<base::Uuid> {
    std::optional<ContextualTask> task =
        CHECK_DEREF(ContextualTasksServiceFactory::GetForProfile(
                        browser()->GetProfile()))
            .GetContextualTaskForTab(
                sessions::SessionTabHelper::IdForTab(moved_contents));
    if (!task.has_value()) {
      return std::nullopt;
    }
    return task->GetTaskId();
  };

  auto get_moved_tab_window = [&moved_contents]() -> BrowserWindowInterface* {
    tabs::TabInterface* const tab =
        tabs::TabInterface::MaybeGetFromContents(moved_contents);
    return tab ? tab->GetBrowserWindowInterface() : nullptr;
  };

  // Returns the task ID in the `kTaskQueryParam` query param of the WebUI URL
  // that the side panel in `window` shows.
  auto get_side_panel_task_param =
      [](BrowserWindowInterface* window) -> std::string {
    content::WebContents* const panel_contents =
        ContextualTasksPanelController::From(window)->GetActiveWebContents();
    std::string task_param;
    if (panel_contents) {
      net::GetValueForKeyInQuery(panel_contents->GetLastCommittedURL(),
                                 kTaskQueryParam, &task_param);
    }
    return task_param;
  };

  RunTestSequence(
      // (1) Open a webpage in the first tab, with a second tab in the window.
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kPageUrl),
      AddInstrumentedTab(kGenericTab, kOtherPageUrl),
      WaitForWebContentsReady(kGenericTab, kOtherPageUrl),
      SelectTab(kTabStripElementId, 0),

      // (2) Click the pinned Contextual Tasks toolbar button.
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),

      // (3) The side panel opens with a task for the active tab.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "MoveTabSidePanelContents",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelId, "MoveTabSidePanelContents"),
      WaitForElementExists(kSidePanelId, kComposebox), Do([&]() {
        moved_contents = browser()->tab_strip_model()->GetActiveWebContents();
        task_id = get_task_id();
      }),
      Check([&]() { return task_id.has_value(); },
            "Active tab is associated with a task"),
      Check(
          [&]() {
            return get_side_panel_task_param(browser()) ==
                   task_id->AsLowercaseString();
          },
          "Side panel shows the active tab's task"),
      // The original window's side panel contents are released after the tab
      // moves away, so stop tracking them first.
      UninstrumentWebContents(kSidePanelId),

      // (4) Move the active tab to a new window.
      InstrumentNextTab(kMovedTabId, AnyBrowser()),
      CheckResult(
          [this]() {
            return chrome::ExecuteCommand(browser(),
                                          IDC_MOVE_TAB_TO_NEW_WINDOW);
          },
          true),

      // (5) The tab opens in a new window and keeps its task.
      InAnyContext(WaitForWebContentsReady(kMovedTabId, kPageUrl)),
      Check(
          [&]() {
            BrowserWindowInterface* const window = get_moved_tab_window();
            return window && window != browser();
          },
          "Moved tab is in a new window"),
      Check([&]() { return get_task_id() == task_id; },
            "Moved tab keeps its task"),
      // The original window is left with the other tab, which has no task, so
      // its side panel closes.
      WaitForHide(kContextualTasksSidePanelWebViewElementId),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      Check(
          [this]() {
            return !ContextualTasksPanelController::From(browser())
                        ->IsPanelOpenForContextualTask();
          },
          "Side panel is closed in the original window"),
      // Side panels belong to a window, so the panel does not move with the
      // tab (crbug.com/534437253).
      Check(
          [&]() {
            return !ContextualTasksPanelController::From(get_moved_tab_window())
                        ->IsPanelOpenForContextualTask();
          },
          "Side panel is not open yet in the new window"),

      // (6) Click the pinned toolbar button in the new window. The steps run
      // in the new window's context, which owns the moved tab.
      Do([&]() {
        SidePanelUI::From(get_moved_tab_window())
            ->DisableAnimationsForTesting();
      }),
      InSameContextAs(
          kMovedTabId,
          WaitForShow(
              kPinnedToolbarActionShowSidePanelContextualTasksElementId),
          PressButton(
              kPinnedToolbarActionShowSidePanelContextualTasksElementId),

          // (7) The side panel opens in the new window. The pinned button
          // starts a new zero-state task for the active tab.
          WaitForShow(kContextualTasksSidePanelWebViewElementId),
          NameViewRelative(
              kContextualTasksSidePanelWebViewElementId,
              "MoveTabNewWindowSidePanelContents",
              [](ContextualTasksWebView* web_view) -> views::View* {
                return web_view->content_web_view();
              }),
          InstrumentNonTabWebView(kNewWindowSidePanelId,
                                  "MoveTabNewWindowSidePanelContents"),
          WaitForElementExists(kNewWindowSidePanelId, kComposebox)),
      Check(
          [&]() {
            return ContextualTasksPanelController::From(get_moved_tab_window())
                ->IsPanelOpenForContextualTask();
          },
          "Side panel is open in the new window"),
      // The original task is not restored in the new window
      // (crbug.com/534437253).
      Check(
          [&]() {
            std::optional<base::Uuid> current_task_id = get_task_id();
            return current_task_id.has_value() && current_task_id != task_id;
          },
          "Pinned button starts a new zero-state task for the moved tab"),
      Check(
          [&]() {
            std::optional<base::Uuid> current_task_id = get_task_id();
            return current_task_id.has_value() &&
                   get_side_panel_task_param(get_moved_tab_window()) ==
                       current_task_id->AsLowercaseString();
          },
          "Side panel in the new window shows the moved tab's task"));
}

// This tests the following CUJ:
//  (1) User has a webpage open in a window with a second tab.
//  (2) User clicks the pinned Contextual Tasks toolbar button.
//  (3) Contextual Tasks side panel opens with a task for the active tab.
//  (4) User closes the active tab with the close tab shortcut (Ctrl+W, or
//      Cmd+W on Mac).
//  (5) The other tab becomes active and the side panel closes.
//  (6) User reopens the closed tab with the restore tab shortcut
//      (Ctrl+Shift+T, or Cmd+Shift+T on Mac).
//  (7) The tab is restored at its URL. The side panel stays closed because the
//      restored tab is not associated with the closed tab's task.
//  (8) User clicks the pinned Contextual Tasks toolbar button.
//  (9) Contextual Tasks side panel opens with a new zero-state task for the
//      restored tab.
IN_PROC_BROWSER_TEST_P(ContextualTasksPinnedToolbarInteractiveUiTest,
                       RestoreClosedTabAndReopenPanel) {
  // Incognito profiles have no TabRestoreService, so the restore tab command
  // is disabled.
  SkipIfIncognito("Reopening closed tabs is not available in Incognito");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kRestoredTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kRestoredTabSidePanelId);

  const GURL kPageUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kOtherPageUrl = embedded_test_server()->GetURL("/title2.html");
  const DeepQuery kComposebox = {"contextual-tasks-app", "#composebox",
                                 "#composebox"};

  PinnedToolbarActionsModel::Get(browser()->GetProfile())
      ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);

  std::optional<base::Uuid> task_id;

  // Returns the ID of the task associated with the active tab.
  auto get_active_tab_task_id = [this]() -> std::optional<base::Uuid> {
    std::optional<ContextualTask> task =
        CHECK_DEREF(ContextualTasksServiceFactory::GetForProfile(
                        browser()->GetProfile()))
            .GetContextualTaskForTab(sessions::SessionTabHelper::IdForTab(
                browser()->tab_strip_model()->GetActiveWebContents()));
    if (!task.has_value()) {
      return std::nullopt;
    }
    return task->GetTaskId();
  };

  // Returns the task ID in the `kTaskQueryParam` query param of the WebUI URL
  // that the side panel shows.
  auto get_side_panel_task_param = [this]() -> std::string {
    content::WebContents* const panel_contents =
        ContextualTasksPanelController::From(browser())->GetActiveWebContents();
    std::string task_param;
    if (panel_contents) {
      net::GetValueForKeyInQuery(panel_contents->GetLastCommittedURL(),
                                 kTaskQueryParam, &task_param);
    }
    return task_param;
  };

  // Sends the keyboard shortcut for `command_id` to the browser window. On
  // Mac, the close and restore tab shortcuts are main menu key equivalents
  // that are not registered with the views FocusManager, so run the command
  // that the shortcut maps to instead.
  auto press_shortcut = [this](int command_id) {
#if BUILDFLAG(IS_MAC)
    return Steps(CheckResult(
        [this, command_id]() {
          return chrome::ExecuteCommand(browser(), command_id);
        },
        true));
#else
    ui::Accelerator accelerator;
    CHECK(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
        command_id, &accelerator));
    return Steps(SendAccelerator(kBrowserViewElementId, accelerator));
#endif
  };

  RunTestSequence(
      // (1) Open a webpage in the first tab, with a second tab in the window.
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kPageUrl),
      AddInstrumentedTab(kGenericTab, kOtherPageUrl),
      WaitForWebContentsReady(kGenericTab, kOtherPageUrl),
      SelectTab(kTabStripElementId, 0),

      // (2) Click the pinned Contextual Tasks toolbar button.
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),

      // (3) The side panel opens with a task for the active tab.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "RestoreTabSidePanelContents",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelId, "RestoreTabSidePanelContents"),
      WaitForElementExists(kSidePanelId, kComposebox),
      Do([&]() { task_id = get_active_tab_task_id(); }),
      Check([&]() { return task_id.has_value(); },
            "Active tab is associated with a task"),
      Check(
          [&]() {
            return get_side_panel_task_param() == task_id->AsLowercaseString();
          },
          "Side panel shows the active tab's task"),
      // The side panel contents are released once no open tab is associated
      // with the task, so stop tracking them before closing the tab.
      UninstrumentWebContents(kSidePanelId),

      // (4) Close the active tab with the close tab shortcut.
      press_shortcut(IDC_CLOSE_TAB),

      // (5) The other tab, which has no task, becomes active and the side panel
      // closes.
      WaitForHide(kContextualTasksSidePanelWebViewElementId),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      CheckResult(
          [this]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL();
          },
          kOtherPageUrl),
      Check(
          [this]() {
            return !ContextualTasksPanelController::From(browser())
                        ->IsPanelOpenForContextualTask();
          },
          "Side panel is closed after the task tab closes"),

      // (6) Reopen the closed tab with the restore tab shortcut.
      InstrumentNextTab(kRestoredTabId), press_shortcut(IDC_RESTORE_TAB),

      // (7) The tab is restored at its URL and becomes active. Task
      // associations are not saved with closed tabs, so the restored tab has
      // no task and the side panel stays closed.
      WaitForWebContentsReady(kRestoredTabId, kPageUrl),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2),
      CheckResult(
          [this]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL();
          },
          kPageUrl),
      Check([&]() { return !get_active_tab_task_id().has_value(); },
            "Restored tab is not associated with a task"),
      Check(
          [this]() {
            return !ContextualTasksPanelController::From(browser())
                        ->IsPanelOpenForContextualTask();
          },
          "Side panel stays closed for the restored tab"),

      // (8) Click the pinned Contextual Tasks toolbar button.
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),

      // (9) The side panel opens with a new zero-state task for the restored
      // tab.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "RestoredTabSidePanelContents",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kRestoredTabSidePanelId,
                              "RestoredTabSidePanelContents"),
      WaitForElementExists(kRestoredTabSidePanelId, kComposebox),
      Check(
          [this]() {
            return ContextualTasksPanelController::From(browser())
                ->IsPanelOpenForContextualTask();
          },
          "Side panel is open for the restored tab"),
      Check(
          [&]() {
            std::optional<base::Uuid> current_task_id =
                get_active_tab_task_id();
            return current_task_id.has_value() && current_task_id != task_id;
          },
          "Pinned button starts a new zero-state task for the restored tab"),
      Check(
          [&]() {
            std::optional<base::Uuid> current_task_id =
                get_active_tab_task_id();
            return current_task_id.has_value() &&
                   get_side_panel_task_param() ==
                       current_task_id->AsLowercaseString();
          },
          "Side panel shows the restored tab's task"));
}

// TODO(crbug.com/500717050): Parameterize this test suite on the feature flag.
// This tests the following CUJ:
//  (1) User opens the Contextual Tasks side panel.
//  (2) User clicks the add context entrypoint in the composebox.
//  (3) User selects "Upload file" and picks a PDF file.
//  (4) User sees the PDF document chip with the file name, and its upload
//      finishes.
//  (5) User clicks the remove button on the document chip.
//  (6) User sees the document chip removed and no attachments remain.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AddAndRemovePdfChipFromComposebox) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("download.pdf");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kDocumentChip = {"contextual-tasks-app",
                                   "#composebox",
                                   "#composebox",
                                   "#carousel",
                                   "cr-composebox-file-thumbnail",
                                   "#documentChip"};

  const DeepQuery kRemoveDocumentButton = {"contextual-tasks-app",
                                           "#composebox",
                                           "#composebox",
                                           "#carousel",
                                           "cr-composebox-file-thumbnail",
                                           "#removeDocumentButton"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      // Wait for the embedded thread frame to finish loading before attaching
      // a file. Its initial navigation can reset the task, which clears any
      // attachment added before it completes on slow bots.
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),

      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "fileUpload"),

      WaitForDocumentChipWithTitle(kSidePanelId, "download.pdf"),
      WaitForElementVisible(kSidePanelId, kDocumentChip),
      // Ensure the upload has settled before removing the chip so the removal
      // does not race with in-flight upload status updates.
      WaitForComposeboxFileUploadsComplete(kSidePanelId, 1),

      WaitForElementVisible(kSidePanelId, kRemoveDocumentButton),
      ClickButton(kSidePanelId, kRemoveDocumentButton),
      WaitForElementDoesNotExist(kSidePanelId, kDocumentChip),
      WaitForComposeboxFilesCount(kSidePanelId, 0));
}

// This tests the following CUJ:
//  (1) User opens the Contextual Tasks side panel.
//  (2) User clicks the add context entrypoint in the composebox.
//  (3) User selects "Upload image" and picks an image file.
//  (4) The image chip appears in the composebox carousel.
//  (5) User clicks the remove button on the image chip.
//  (6) The image chip is removed and no attachments remain.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AddAndRemoveImageChipFromComposebox) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("handbag.png");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kImgChip = {
      "contextual-tasks-app",         "#composebox", "#composebox", "#carousel",
      "cr-composebox-file-thumbnail", "#imgChip"};

  const DeepQuery kRemoveImgButton = {"contextual-tasks-app",
                                      "#composebox",
                                      "#composebox",
                                      "#carousel",
                                      "cr-composebox-file-thumbnail",
                                      "#removeImgButton"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      // Wait for the thread webview to load before attaching an image. During
      // zero-state initialization, ContextualTasksAppElement (app.ts) calls
      // forceComposeboxFocus(), which clears manually added attachments,
      // before loading the thread URL. An image attached before then would be
      // silently removed.
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),

      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "imageUpload"),

      WaitForElementVisible(kSidePanelId, kImgChip),
      WaitForComposeboxFilesCount(kSidePanelId, 1),

      ClickButton(kSidePanelId, kRemoveImgButton),
      WaitForElementDoesNotExist(kSidePanelId, kImgChip),
      WaitForComposeboxFilesCount(kSidePanelId, 0));
}

// This tests the following CUJ:
//  (1) User opens a background tab and navigates to a webpage.
//  (2) User opens the Contextual Tasks side panel on the primary tab.
//  (3) User clicks the add context entrypoint in the composebox and selects the
//      background tab from the menu.
//  (4) The tab favicon chip appears in the composebox.
//  (5) User reopens the context menu and deselects the tab.
//  (6) The tab favicon chip is removed and no attachments remain.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AddAndRemoveTabFromComposebox) {
  SkipIfIncognito("Tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      NavigateWebContents(kPrimaryTab, GURL(chrome::kChromeUIVersionURL)),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl),
      WaitForWebContentsReady(kGenericTab, kGenericPageUrl),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),

      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, 0),

      WaitForFaviconGroupWithTitle(kSidePanelId, "title1.html"),
      WaitForComposeboxFilesCount(kSidePanelId, 1),

      ForceClickAddContextEntrypoint(kSidePanelId),
      WaitForMenuTabSelected(kSidePanelId, 0),
      ForceClickMenuButton(kSidePanelId, 0),

      WaitForElementDoesNotExist(kSidePanelId, kFaviconGroup),
      WaitForComposeboxFilesCount(kSidePanelId, 0));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AddAndSubmitTabFromComposebox) {
  SkipIfIncognito("Tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      NavigateWebContents(kPrimaryTab, GURL(chrome::kChromeUIVersionURL)),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl),
      WaitForWebContentsReady(kGenericTab, kGenericPageUrl),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, 0),
      WaitForFaviconGroupWithTitle(kSidePanelId, "title1.html"),
      WaitForComposeboxFilesCount(kSidePanelId, 1),
      WaitForSubmitButtonEnabled(kSidePanelId),
      ClickButton(kSidePanelId, kSubmitButton),
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          std::nullopt, 0,
          lens::LensOverlayContextualInputUploadType::
              CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AddAndSubmitPdfChipFromComposebox) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("download.pdf");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kDocumentChip = {"contextual-tasks-app",
                                   "#composebox",
                                   "#composebox",
                                   "#carousel",
                                   "cr-composebox-file-thumbnail",
                                   "#documentChip"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  const DeepQuery kDocumentChipTitle = {"contextual-tasks-app",
                                        "#composebox",
                                        "#composebox",
                                        "#carousel",
                                        "cr-composebox-file-thumbnail",
                                        "#documentTitle"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "fileUpload"),
      WaitForDocumentChipWithTitle(kSidePanelId, "download.pdf"),
      WaitForComposeboxFilesCount(kSidePanelId, 1),
      WaitForSubmitButtonEnabled(kSidePanelId),
      ClickButton(kSidePanelId, kSubmitButton),
      VerifySubmitQueryMessage(lens::LensOverlayRequestId::MEDIA_TYPE_PDF));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AddAndSubmitImageChipFromComposebox) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("handbag.png");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kImgChip = {
      "contextual-tasks-app",         "#composebox", "#composebox", "#carousel",
      "cr-composebox-file-thumbnail", "#imgChip"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "imageUpload"),
      WaitForElementExists(kSidePanelId, kImgChip),
      WaitForComposeboxFilesCount(kSidePanelId, 1),
      WaitForSubmitButtonEnabled(kSidePanelId),
      ClickButton(kSidePanelId, kSubmitButton),
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AddAndSubmitMultipleContextsFromComposebox) {
  SkipIfIncognito("Tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kGenericPageUrl2 = embedded_test_server()->GetURL("/title2.html");

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath pdf_path = test_data_dir.AppendASCII("download.pdf");
  base::FilePath image_path = test_data_dir.AppendASCII("handbag.png");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      NavigateWebContents(kPrimaryTab, GURL(chrome::kChromeUIVersionURL)),
      AddInstrumentedTab(kGenericTab2, kGenericPageUrl2),
      WaitForWebContentsReady(kGenericTab2, kGenericPageUrl2),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl1),
      WaitForWebContentsReady(kGenericTab, kGenericPageUrl1),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),

      // 1. Add Tab 1 (most recent since we opened Tab 2 first, is at Index 0)
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, 0),
      WaitForFaviconGroupWithTitle(kSidePanelId, "title1.html"),
      WaitForComposeboxFilesCount(kSidePanelId, 1),

      // 2. Add Tab 2
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, 1),
      WaitForFaviconGroupWithTitle(kSidePanelId, "Title Of Awesomeness"),
      WaitForComposeboxFilesCount(kSidePanelId, 2),

      // 3. Set factory for PDF and upload PDF
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{pdf_path}));
      })),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "fileUpload"),
      WaitForDocumentChipWithTitle(kSidePanelId, "download.pdf"),
      WaitForComposeboxFilesCount(kSidePanelId, 3),

      // 4. Set factory for Image 1 and upload Image 1
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{image_path}));
      })),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "imageUpload"),
      WaitForComposeboxFilesCount(kSidePanelId, 4),

      // 5. Set factory for Image 2 and upload Image 2
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{image_path}));
      })),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "imageUpload"),
      WaitForComposeboxFilesCount(kSidePanelId, 5),

      // 6. Submit
      WaitForSubmitButtonEnabled(kSidePanelId),
      ClickButton(kSidePanelId, kSubmitButton),

      // 7. Verify multiple inputs in the final message
      // We expect 4 images: 2 manual images + 2 viewports from the 2 tabs.
      // (PDF has no viewport screenshot since it's uploaded as a manual file).
      VerifyMultipleSubmitQueryMessage(
          /*expected_query_text=*/"",
          /*expected_viewport_image_count=*/2,
          /*expected_upload_image_count=*/2,
          /*expected_upload_file_count=*/1, std::vector<std::string>{}));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AddAndSubmitTextOnlyFromComposebox) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),

      // Type text query
      InputText(kSidePanelId, "My text-only query"),

      // Click Submit
      WaitForSubmitButtonEnabled(kSidePanelId),
      ClickButton(kSidePanelId, kSubmitButton),

      // Verify query submission (0 viewports, 0 uploads, 0 files, expected
      // text)
      VerifyMultipleSubmitQueryMessage("My text-only query",
                                       /*expected_viewport_image_count=*/0,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{}),

      // After submit the compsebox input should be empty.
      WaitForInputCleared(kSidePanelId));
}

// TODO(https://crbug.com/568329503): This is flaky/fails on at least some Linux
// builders.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_AddAndSubmitMultipleContextsWithTextFromComposebox \
  DISABLED_AddAndSubmitMultipleContextsWithTextFromComposebox
#else
#define MAYBE_AddAndSubmitMultipleContextsWithTextFromComposebox \
  AddAndSubmitMultipleContextsWithTextFromComposebox
#endif
IN_PROC_BROWSER_TEST_P(
    ContextualTasksInteractiveUiTest,
    MAYBE_AddAndSubmitMultipleContextsWithTextFromComposebox) {
  SkipIfIncognito("Tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kGenericPageUrl2 = embedded_test_server()->GetURL("/title2.html");

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath pdf_path = test_data_dir.AppendASCII("download.pdf");
  base::FilePath image_path = test_data_dir.AppendASCII("handbag.png");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      NavigateWebContents(kPrimaryTab, GURL(chrome::kChromeUIVersionURL)),
      AddInstrumentedTab(kGenericTab2, kGenericPageUrl2),
      WaitForWebContentsReady(kGenericTab2, kGenericPageUrl2),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl1),
      WaitForWebContentsReady(kGenericTab, kGenericPageUrl1),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),

      // 1. Add Tab 1
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, 0),
      WaitForFaviconGroupWithTitle(kSidePanelId, "title1.html"),
      WaitForComposeboxFilesCount(kSidePanelId, 1),

      // 2. Add Tab 2
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, 1),
      WaitForFaviconGroupWithTitle(kSidePanelId, "Title Of Awesomeness"),
      WaitForComposeboxFilesCount(kSidePanelId, 2),

      // 3. Set factory for PDF and upload PDF
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{pdf_path}));
      })),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "fileUpload"),
      WaitForDocumentChipWithTitle(kSidePanelId, "download.pdf"),
      WaitForComposeboxFilesCount(kSidePanelId, 3),

      // 4. Set factory for Image 1 and upload Image 1
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{image_path}));
      })),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "imageUpload"),
      WaitForComposeboxFilesCount(kSidePanelId, 4),

      // 5. Set factory for Image 2 and upload Image 2
      Do(base::BindLambdaForTesting([&]() {
        ui::SelectFileDialog::SetFactory(
            std::make_unique<content::FakeSelectFileDialogFactory>(
                std::vector<base::FilePath>{image_path}));
      })),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "imageUpload"),
      WaitForComposeboxFilesCount(kSidePanelId, 5),

      // Type query text
      InputText(kSidePanelId, "Query with multiple attachments"),

      // 6. Submit
      WaitForSubmitButtonEnabled(kSidePanelId),
      ClickButton(kSidePanelId, kSubmitButton),

      // 7. Verify multiple inputs + query text in the final message
      VerifyMultipleSubmitQueryMessage("Query with multiple attachments",
                                       /*expected_viewport_image_count=*/2,
                                       /*expected_upload_image_count=*/2,
                                       /*expected_upload_file_count=*/1,
                                       std::vector<std::string>{}));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AutoSuggestedTabChipAppearsAndCanBeSubmitted) {
  SkipIfIncognito(
      "Auto-suggested tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for composebox WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      // Navigate active tab to a valid page.
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // The navigated active tab (title1.html) should be automatically
      // suggested and displayed inside the favicon coin group.
      WaitForFaviconGroupWithTitle(kSidePanelWebContentsId, "title1.html"),

      // Click the submit button.
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify the sent query includes the auto-suggested tab context.
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          std::nullopt, 0,
          lens::LensOverlayContextualInputUploadType::
              CONTEXTUAL_INPUT_UPLOAD_TYPE_AUTO_TAB_CHIP));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       AutoSuggestedTabChipCanBeDismissed) {
  SkipIfIncognito(
      "Auto-suggested tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  StateChange coin_disappeared;
  coin_disappeared.type = StateChange::Type::kExistsAndConditionTrue;
  coin_disappeared.where = {"contextual-tasks-app"};
  coin_disappeared.test_function =
      "function(app) {"
      "  const cb = "
      "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
      "querySelector('#composebox');"
      "  if (cb) {"
      "    document.getAnimations({subtree: true}).forEach(a => a.finish());"
      "  }"
      "  const el = "
      "cb?.shadowRoot?.querySelector('#contextEntrypoint')?.shadowRoot?."
      "querySelector('#entrypointButton')?.shadowRoot?.querySelector('"
      "composebox-favicon-group');"
      "  return !el;"
      "}";
  coin_disappeared.event = kElementDoesNotExistEvent;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for composebox WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      // Navigate active tab to a valid page.
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Wait for the suggestion coin group to appear.
      WaitForElementExists(kSidePanelWebContentsId, kFaviconGroup),

      // Dismiss the suggested tab by opening context entrypoint and deselecting
      // the tab.
      ForceClickAddContextEntrypoint(kSidePanelWebContentsId),
      ForceClickMenuButton(kSidePanelWebContentsId, 0),

      // Verify the dropdown menu has enableMultiTabSelection_ set to true.
      CheckJsResult(
          kSidePanelWebContentsId,
          "() => { "
          "const app = document.querySelector('contextual-tasks-app');"
          "const cb = "
          "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
          "querySelector('#composebox');"
          "const entry = cb?.shadowRoot?.querySelector('#contextEntrypoint'); "
          "const m = entry?.shadowRoot?.querySelector('#menu'); "
          "return m?.enableMultiTabSelection_ === true; }"),

      // Verify the composebox files list successfully drops to 0.
      CheckJsResult(
          kSidePanelWebContentsId,
          "() => { "
          "const app = document.querySelector('contextual-tasks-app');"
          "const cb = "
          "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
          "querySelector('#composebox');"
          "const triggerTabId = cb?.tabSuggestions?.[0]?.tabId; "
          "const tokenFromMap = cb?.addedTabsIds?.get(triggerTabId); "
          "const hasTokenInFiles = cb?.attachedContext?.has(tokenFromMap); "
          "return `hasToken: ${hasTokenInFiles}, size: ${cb?.attachedContext?.size}, "
          "filesKeys: ${Array.from(cb?.attachedContext?.keys()).map(k => k.high + '_' + "
          "k.low)}, mapVal: ${tokenFromMap ? tokenFromMap.high + '_' + "
          "tokenFromMap.low : 'null'}`; }",
          "hasToken: false, size: 0, filesKeys: , mapVal: null"),

      // Wait for the favicon coin group to disappear robustly.
      WaitForStateChange(kSidePanelWebContentsId, coin_disappeared),

      // Await a deterministic Mojo roundtrip to guarantee the DeleteContext
      // Mojo call has finished executing in C++.
      CheckJsResult(
          kSidePanelWebContentsId,
          "() => { "
          "const app = document.querySelector('contextual-tasks-app');"
          "const cb = "
          "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
          "querySelector('#composebox');"
          "return cb?.searchboxHandler_.getInputState().then(() => true); }"),

      // Navigate away and back to trigger context updates.
      NavigateWebContents(kPrimaryTab, GURL("about:blank")),
      WaitForElementDoesNotExist(kSidePanelWebContentsId, kFaviconGroup),
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Verify the suggestion chip does not reappear because it was dismissed.
      EnsureNotPresent(kSidePanelWebContentsId, kFaviconGroup));
}

class ContextualTasksInteractiveUiTestWithChips
    : public ContextualTasksInteractiveUiTest {
 public:
  ContextualTasksInteractiveUiTestWithChips() = default;
  ~ContextualTasksInteractiveUiTestWithChips() override = default;

  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    auto disabled = GetDefaultDisabledFeatures();
    disabled.push_back(omnibox::kTabFaviconChipsToCoins);
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }
};

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestWithChips,
                       AutoSuggestedTabChipAppearsAndCanBeSubmitted) {
  SkipIfIncognito(
      "Auto-suggested tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kTabChip = {
      "contextual-tasks-app",         "#composebox", "#composebox", "#carousel",
      "cr-composebox-file-thumbnail", "#tabChip"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  StateChange files_ready;
  files_ready.type = StateChange::Type::kExistsAndConditionTrue;
  files_ready.where = {"contextual-tasks-app", "#composebox", "#composebox"};
  files_ready.test_function =
      "el => el.attachedContext && el.attachedContext.size === 1";
  files_ready.event = kElementExistsEvent;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for composebox WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      // Navigate active tab to a valid page.
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Wait asynchronously for WebUI files list to capture the tab addition
      // Mojo callback.
      WaitForStateChange(kSidePanelWebContentsId, files_ready),

      // The navigated active tab (title1.html) should be automatically
      // suggested as a standard carousel chip.
      WaitForTabChipWithTitle(kSidePanelWebContentsId, "title1.html"),

      // Click the submit button.
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify the sent query includes the auto-suggested tab context.
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          std::nullopt, 0,
          lens::LensOverlayContextualInputUploadType::
              CONTEXTUAL_INPUT_UPLOAD_TYPE_AUTO_TAB_CHIP));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestWithChips,
                       AutoSuggestedTabChipCanBeDismissed) {
  SkipIfIncognito(
      "Auto-suggested tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kTabChip = {
      "contextual-tasks-app",         "#composebox", "#composebox", "#carousel",
      "cr-composebox-file-thumbnail", "#tabChip"};

  const DeepQuery kRemoveTabButton = {"contextual-tasks-app",
                                      "#composebox",
                                      "#composebox",
                                      "#carousel",
                                      "cr-composebox-file-thumbnail",
                                      "#removeTabButton"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  StateChange files_ready;
  files_ready.type = StateChange::Type::kExistsAndConditionTrue;
  files_ready.where = {"contextual-tasks-app", "#composebox", "#composebox"};
  files_ready.test_function =
      "el => el.attachedContext && el.attachedContext.size === 1";
  files_ready.event = kElementExistsEvent;

  StateChange chip_disappeared;
  chip_disappeared.type = StateChange::Type::kExistsAndConditionTrue;
  chip_disappeared.where = {"contextual-tasks-app"};
  chip_disappeared.test_function =
      "function(app) {"
      "  const cb = "
      "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
      "querySelector('#composebox');"
      "  if (cb) {"
      "    document.getAnimations({subtree: true}).forEach(a => a.finish());"
      "  }"
      "  const el = "
      "cb?.shadowRoot?.querySelector('#carousel')?.shadowRoot?."
      "querySelector('cr-composebox-file-thumbnail')?.shadowRoot?."
      "querySelector('#tabChip');"
      "  return !el;"
      "}";
  chip_disappeared.event = kElementDoesNotExistEvent;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for composebox WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Wait asynchronously for WebUI files list to capture the tab addition
      // Mojo callback.
      WaitForStateChange(kSidePanelWebContentsId, files_ready),

      // Wait for the suggestion chip to appear.
      WaitForElementExists(kSidePanelWebContentsId, kTabChip),

      // Dismiss the suggested tab chip by clicking the remove button inside
      // carousel.
      ClickButton(kSidePanelWebContentsId, kRemoveTabButton),

      // Wait for the suggested tab chip to disappear robustly.
      WaitForStateChange(kSidePanelWebContentsId, chip_disappeared),

      // Await a deterministic Mojo roundtrip to guarantee the DeleteContext
      // Mojo call has finished executing in C++.
      CheckJsResult(
          kSidePanelWebContentsId,
          "() => { "
          "const app = document.querySelector('contextual-tasks-app');"
          "const cb = "
          "app?.shadowRoot?.querySelector('#composebox')?.shadowRoot?."
          "querySelector('#composebox');"
          "return cb?.searchboxHandler_.getInputState().then(() => true); }"),

      // Navigate away and back to trigger context updates.
      NavigateWebContents(kPrimaryTab, GURL("about:blank")),
      WaitForElementDoesNotExist(kSidePanelWebContentsId, kTabChip),
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Verify the suggestion chip does not reappear because it was dismissed.
      EnsureNotPresent(kSidePanelWebContentsId, kTabChip));
}

class ContextualTasksInteractiveUiTestParameterized
    : public ContextualTasksInteractiveUiTestBase,
      public testing::WithParamInterface<std::tuple<UserVariation, bool>> {
 public:
  ContextualTasksInteractiveUiTestParameterized() = default;
  ~ContextualTasksInteractiveUiTestParameterized() override = default;

  bool GetAimTriggeredThreadLinksParam() const {
    return std::get<1>(GetParam());
  }

  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    auto disabled = GetDefaultDisabledFeatures();
    if (GetAimTriggeredThreadLinksParam()) {
      enabled.push_back({kAimTriggeredThreadLinks, {}});
    } else {
      disabled.push_back(kAimTriggeredThreadLinks);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContextualTasksInteractiveUiTestBase::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);

    // Add command line to allow opaque origin post messages to be accepted in
    // GetGuestForMessage. For some reason, during testing, the
    // accounts.google.com postMessage is always opaque, so this is needed for
    // the test to pass.
    command_line->AppendSwitch(
        "allow-opaque-origin-for-contextual-tasks-testing");
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    ContextualTasksInteractiveUiTestBase::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    base::FilePath file_path = test_data_dir.AppendASCII("mock_aim_page.html");
    std::string mock_aim_page_content;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::ReadFileToString(file_path, &mock_aim_page_content));
    }

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        [](std::string mock_aim_page_content,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html; charset=utf-8");
          response->AddCustomHeader("Cross-Origin-Opener-Policy",
                                    "unsafe-none");

          auto it = request.headers.find("Host");
          if (it != request.headers.end()) {
            if (it->second == "myaccount.google.com" &&
                request.relative_url == "/title1.html") {
              response->set_content(
                  "<html><body>Title 1 HTML Page</body></html>");
              return response;
            }
            if (it->second == kMockAimPageHost) {
              response->set_content(mock_aim_page_content);
              return response;
            }
          }
          return nullptr;
        },
        mock_aim_page_content));

    ASSERT_TRUE(https_server_.Start());
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 protected:
  UserVariation GetUserVariation() const override {
    return std::get<0>(GetParam());
  }
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// CUJ covered by this test:
// 1) Opens Contextual Tasks in a tab.
// 2) In the Contextual Tasks <webview>, calls window.open() to a URL (not
// about:blank) with target _blank.
// 3) Verifies that the call returns a window
// object.
// 4) Verifies that the window.open call was successful by verifying the
// new tab opened.
// 5) Back in Contextual Tasks, calls window.open with a URL and
// target _self.
// 6) Verifies the Contextual Tasks tab navigates to the opened
// URL.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestParameterized,
                       WindowOpenCUJ) {
  // TODO(crbug.com/533072196): Clean up this test once the pre-rearchitecture
  // logic is cleaned up.
  SkipIfNotSignedIn(
      "Webview window.open and tab-strip opener routing tests require "
      "Contextual Tasks in a top-level browser tab, which requires a "
      "signed-in session.");

  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kTargetUrl("https://a.google.com/title1.html");

  auto sequence = Steps(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      // 1) Opens Contextual Tasks in a tab.
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      WithElement(kInnerWebContentsId, [kTargetUrl](ui::TrackedElement* el) {
        auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
        content::TestNavigationObserver nav_observer(kTargetUrl);
        nav_observer.StartWatchingNewWebContents();

        // 2) In the Contextual Tasks <webview>, calls window.open() to a
        // URL (not about:blank) with target _blank. 3) Verifies that the
        // call returns a window object.
        bool returns_window =
            content::EvalJs(
                web_contents,
                base::StringPrintf("window.open('%s', '_blank') !== null",
                                   kTargetUrl.spec().c_str()))
                .ExtractBool();
        EXPECT_TRUE(returns_window);

        // 4) Verifies that the window.open call was successful by verifying
        // the new tab opened.
        nav_observer.Wait();
      }));

  if (GetAimTriggeredThreadLinksParam()) {
    // When flag is enabled, the source tab stays open.
    // Instrument the new tab at index 1.
    sequence = Steps(
        std::move(sequence), InstrumentTab(kNewTabId, 1),
        SelectTab(kTabStripElementId, 0), WaitForShow(kInnerWebContentsId),
        // 5) Back in Contextual Tasks, calls window.open with a URL and target
        // _self.
        Do(base::BindLambdaForTesting([this, kTargetUrl]() {
          auto* web_contents =
              browser()->tab_strip_model()->GetActiveWebContents();
          auto inner_contents = web_contents->GetInnerWebContents();
          ASSERT_FALSE(inner_contents.empty());
          auto* guest_contents = inner_contents[0];

          content::ExecuteScriptAsync(
              guest_contents, base::StringPrintf("window.open('%s', '_self')",
                                                 kTargetUrl.spec().c_str()));
        })),
        // 6) Verifies the Contextual Tasks tab navigates to the opened URL.
        WaitForWebContentsNavigation(kPrimaryTab, kTargetUrl));
  } else {
    // When flag is disabled, the source tab closes and the new tab becomes
    // index 0.
    sequence = Steps(
        std::move(sequence), InstrumentTab(kNewTabId, 0),

        // Verifies the new tab opened and is at the target URL.
        CheckElement(kNewTabId,
                     base::BindOnce(
                         [](const GURL& expected_url, ui::TrackedElement* el) {
                           return AsInstrumentedWebContents(el)
                                      ->web_contents()
                                      ->GetLastCommittedURL() == expected_url;
                         },
                         kTargetUrl)));
  }

  RunTestSequence(std::move(sequence));
}

// CUJ covered by this test:
// 1) Opens Contextual Tasks in a tab.
// 2) In the Contextual Tasks <webview>, calls window.open() to a URL (not
// about:blank) with target _blank.
// 3) Verifies that the call returns a window
// object.
// 4) Verifies that the window.open call was successful by verifying the
// new tab opened.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestParameterized,
                       WindowOpenBlank) {
  // TODO(crbug.com/533072196): Clean up this test once the pre-rearchitecture
  // logic is cleaned up.
  SkipIfNotSignedIn(
      "Webview window.open and tab-strip opener routing tests require "
      "Contextual Tasks in a top-level browser tab, which requires a "
      "signed-in session.");

  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kTargetUrl("https://a.google.com/title1.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      // 1) Opens Contextual Tasks in a tab.
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      WithElement(kInnerWebContentsId, [kTargetUrl](ui::TrackedElement* el) {
        auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
        content::TestNavigationObserver nav_observer(kTargetUrl);
        nav_observer.StartWatchingNewWebContents();

        // 2) In the Contextual Tasks <webview>, calls window.open() to a URL
        // (not about:blank) with target _blank. 3) Verifies that the call
        // returns a window object.
        bool returns_window =
            content::EvalJs(
                web_contents,
                base::StringPrintf("window.open('%s', '_blank') !== null",
                                   kTargetUrl.spec().c_str()))
                .ExtractBool();
        EXPECT_TRUE(returns_window);

        // 4) Verifies that the window.open call was successful by verifying the
        // new tab opened.
        nav_observer.Wait();
      }));
}

// CUJ covered by this test:
// 1) Opens Contextual Tasks in a tab.
// 2) Back in Contextual Tasks, calls window.open with a URL and target _self.
// 3) Verifies the Contextual Tasks tab navigates to the opened URL.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestParameterized,
                       WindowOpenSelf) {
  // TODO(crbug.com/533072196): Clean up this test once the pre-rearchitecture
  // logic is cleaned up.
  SkipIfNotSignedIn(
      "Webview window.open and tab-strip opener routing tests require "
      "Contextual Tasks in a top-level browser tab, which requires a "
      "signed-in session.");

  if (!GetAimTriggeredThreadLinksParam()) {
    GTEST_SKIP() << "WindowOpenSelf target=_self tab navigation requires "
                    "kAimTriggeredThreadLinks";
  }
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");
  const GURL kTargetUrl("https://a.google.com/title1.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      // 1) Opens Contextual Tasks in a tab.
      OpenContextualTasksInCurrentTab(kInterceptionUrl),
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      // 2) Back in Contextual Tasks, calls window.open with a URL and target
      // _self.
      Do(base::BindLambdaForTesting([this, kTargetUrl]() {
        auto* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        auto inner_contents = web_contents->GetInnerWebContents();
        ASSERT_FALSE(inner_contents.empty());
        auto* guest_contents = inner_contents[0];

        content::ExecuteScriptAsync(
            guest_contents, base::StringPrintf("window.open('%s', '_self')",
                                               kTargetUrl.spec().c_str()));
      })),

      // 3) Verifies the Contextual Tasks tab navigates to the opened URL.
      WaitForWebContentsNavigation(kPrimaryTab, kTargetUrl));
}

// CUJ covered by this test:
// 1) Opens Contextual Tasks in a tab.
// 2) Call window.open from the Contextual Tasks <webview>.
// 3) In the opened window, call window.opener.postMessage.
// 4) Verify the postMessage is received in the <webview>.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestParameterized,
                       PostMessageToDummyOpenerRoutedToWebview) {
  // TODO(crbug.com/533072196): Clean up this test once the pre-rearchitecture
  // logic is cleaned up.
  SkipIfNotSignedIn(
      "Webview window.open and tab-strip opener routing tests require "
      "Contextual Tasks in a top-level browser tab, which requires a "
      "signed-in session.");

  const GURL kActiveTabUrl =
      https_server_.GetURL("myaccount.google.com", "/title1.html");
  const GURL clicked_url =
      https_server_.GetURL("myaccount.google.com", "/title1.html#citation");
  const GURL kInterceptionUrl =
      https_server_.GetURL(kMockAimPageHost, "/search?udm=50");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOpenedTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab2);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInnerWebContentsId2);

  // Step 1: Open Contextual Tasks in a Tab
  auto sequence =
      Steps(InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
            OpenContextualTasksInCurrentTab(kInterceptionUrl),
            InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0));

  sequence = Steps(
      std::move(sequence),
      // 1. Inject listener in the <webview> to collect
      // received messages.
      WithElement(kInnerWebContentsId,
                  base::BindOnce([](ui::TrackedElement* el) {
                    std::string setup_listener = R"(
            window.receivedMessages = [];
            window.messagePromiseResolver = null;
            window.addEventListener('message', (event) => {
              window.receivedMessages.push(event.data);
              if (window.messagePromiseResolver &&
                  event.data === 'hello_from_opened_page') {
                window.messagePromiseResolver(true);
              }
            });
          )";
                    auto* wc = AsInstrumentedWebContents(el)->web_contents();
                    EXPECT_TRUE(content::ExecJs(wc, setup_listener));
                  })),

      // 2. Set up instrumenting the next tab BEFORE we trigger window.open to
      // avoid race conditions.
      InstrumentNextTab(kOpenedTab),

      // 3. Within the <webview>, inject and open a window.
      WithElement(kInnerWebContentsId,
                  base::BindOnce(
                      [](GURL url, ui::TrackedElement* el) {
                        auto* wc =
                            AsInstrumentedWebContents(el)->web_contents();
                        std::string click_script = content::JsReplace(
                            R"(
                  (() => {
                    window.open($1, '_blank');
                  })();
                )",
                            url.spec());
                        EXPECT_TRUE(content::ExecJs(wc, click_script));
                      },
                      clicked_url)),

      // 4. Wait for the opened tab to finish loading
      WaitForWebContentsNavigation(kOpenedTab, clicked_url),

      // 5. Verify window.opener is non-null and call postMessage from the
      // opened tab.
      WithElement(kOpenedTab, base::BindOnce([](ui::TrackedElement* el) {
                    auto* wc = AsInstrumentedWebContents(el)->web_contents();
                    std::string post_message_script = R"(
            (async () => {
              if (!window.opener) {
                return "no opener";
              }
              try {
                window.opener.postMessage("hello_from_opened_page", "*");
                return "ok";
              } catch (e) {
                return "error: " + e.message;
              }
            })();
          )";
                    EXPECT_EQ("ok", content::EvalJs(wc, post_message_script));
                  })),

      // Switch back to the original tab.
      SelectTab(kTabStripElementId, 0));

  const std::string check_message_script = R"(
          new Promise((resolve) => {
            if (window.receivedMessages &&
                window.receivedMessages.includes("hello_from_opened_page")) {
              resolve(true);
            } else {
              window.messagePromiseResolver = resolve;
            }
          });
        )";

  // If AimTriggeredThreadLinks is enabled, the window.open call opens in a new
  // tab, so the original contextual tasks tab still exists.
  if (GetAimTriggeredThreadLinksParam()) {
    sequence = Steps(
        std::move(sequence), WaitForShow(kInnerWebContentsId),
        WithElement(kInnerWebContentsId,
                    base::BindOnce(
                        [](std::string script, ui::TrackedElement* el) {
                          auto* wc =
                              AsInstrumentedWebContents(el)->web_contents();
                          EXPECT_EQ(true, content::EvalJs(wc, script));
                        },
                        check_message_script)));
  } else {
    // If AimTriggeredThreadLinks is disabled, the window.open call moves
    // Contextual Tasks into the side panel. Therefore, instrument the side
    // panel <webview> and ensure the postMessage was received.
    sequence = Steps(
        std::move(sequence),
        WaitForShow(kContextualTasksSidePanelWebViewElementId),
        NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                         "SidePanelContentWebViewName",
                         [](ContextualTasksWebView* web_view) -> views::View* {
                           return web_view->content_web_view();
                         }),
        InstrumentNonTabWebView(kPrimaryTab2, "SidePanelContentWebViewName"),
        InstrumentInnerWebContents(kInnerWebContentsId2, kPrimaryTab2, 0),
        WaitForShow(kInnerWebContentsId2),
        WithElement(kInnerWebContentsId2,
                    base::BindOnce(
                        [](std::string script, ui::TrackedElement* el) {
                          auto* wc =
                              AsInstrumentedWebContents(el)->web_contents();
                          EXPECT_EQ(true, content::EvalJs(wc, script));
                        },
                        check_message_script)));
  }

  RunTestSequence(std::move(sequence));
}

// CUJ covered by this test:
// 1) Opens Contextual Tasks in a tab.
// 2) Call window.open from the Contextual Tasks <webview>.
// 3) In the opened window, call window.opener.postMessage.
// 4) Verify the postMessage is received in the <webview>.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestParameterized,
                       PopupPostMessageToOpenerRoutedToWebview) {
  // TODO(crbug.com/533072196): Clean up this test once the pre-rearchitecture
  // logic is cleaned up.
  SkipIfNotSignedIn(
      "Webview window.open and tab-strip opener routing tests require "
      "Contextual Tasks in a top-level browser tab, which requires a "
      "signed-in session.");

  const GURL kActiveTabUrl =
      https_server_.GetURL("myaccount.google.com", "/title1.html");
  const GURL clicked_url =
      https_server_.GetURL("myaccount.google.com", "/title1.html#citation");
  const GURL kInterceptionUrl =
      https_server_.GetURL(kMockAimPageHost, "/search?udm=50");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOpenedPopup);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab2);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInnerWebContentsId2);

  // Step 1: Open Contextual Tasks in a Tab
  auto sequence =
      Steps(InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
            OpenContextualTasksInCurrentTab(kInterceptionUrl),
            InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0));

  sequence = Steps(
      std::move(sequence),
      // 1. Inject listener in the <webview> to collect
      // received messages.
      WithElement(kInnerWebContentsId,
                  base::BindOnce([](ui::TrackedElement* el) {
                    std::string setup_listener = R"(
            window.receivedMessages = [];
            window.messagePromiseResolver = null;
            window.addEventListener('message', (event) => {
              window.receivedMessages.push(event.data);
              if (window.messagePromiseResolver &&
                  event.data === 'hello_from_opened_page') {
                window.messagePromiseResolver(true);
              }
            });
          )";
                    auto* wc = AsInstrumentedWebContents(el)->web_contents();
                    EXPECT_TRUE(content::ExecJs(wc, setup_listener));
                  })),

      // 2. Set up instrumenting the next tab BEFORE we trigger window.open to
      // avoid race conditions. We use AnyBrowser() to catch it if it opens in
      // a new window.
      InstrumentNextTab(kOpenedPopup, AnyBrowser()),

      // 3. Within the guest view, call window.open with popup=yes
      WithElement(kInnerWebContentsId,
                  base::BindOnce(
                      [](GURL url, ui::TrackedElement* el) {
                        auto* wc =
                            AsInstrumentedWebContents(el)->web_contents();
                        std::string open_script = content::JsReplace(
                            R"(
                  (() => {
                    window.open($1, '_blank', 'popup=yes,width=400,height=400');
                  })();
                )",
                            url.spec());
                        EXPECT_TRUE(content::ExecJs(wc, open_script));
                      },
                      clicked_url)),

      // 4. Wait for the opened popup to finish loading
      WaitForWebContentsNavigation(kOpenedPopup, clicked_url),

      // Check that the popup window is focused when it is opened.
      CheckElement(
          kOpenedPopup, base::BindOnce([](ui::TrackedElement* el) {
            auto* wc = AsInstrumentedWebContents(el)->web_contents();
            tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(wc);
            BrowserWindowInterface* popup_browser =
                tab->GetBrowserWindowInterface();
            EXPECT_TRUE(popup_browser);
            EXPECT_TRUE(ui_test_utils::IsBrowserActive(popup_browser));
            return true;
          })),

      // 5. Verify window.opener is non-null and call postMessage from the
      // opened popup
      WithElement(kOpenedPopup, base::BindOnce([](ui::TrackedElement* el) {
                    auto* wc = AsInstrumentedWebContents(el)->web_contents();
                    std::string post_message_script = R"(
            (async () => {
              if (!window.opener) {
                return "no opener";
              }
              try {
                window.opener.postMessage("hello_from_opened_page", "*");
                return "ok";
              } catch (e) {
                return "error: " + e.message;
              }
            })();
          )";
                    EXPECT_EQ("ok", content::EvalJs(wc, post_message_script));
                  })));

  const std::string check_message_script = R"(
          new Promise((resolve) => {
            if (window.receivedMessages &&
                window.receivedMessages.includes("hello_from_opened_page")) {
              resolve(true);
            } else {
              window.messagePromiseResolver = resolve;
            }
          });
        )";

  // If AimTriggeredThreadLinks is enabled, the window.open call opens in a new
  // tab, so the original contextual tasks tab still exists.
  if (GetAimTriggeredThreadLinksParam()) {
    sequence = Steps(
        std::move(sequence), WaitForShow(kInnerWebContentsId),
        WithElement(kInnerWebContentsId,
                    base::BindOnce(
                        [](std::string script, ui::TrackedElement* el) {
                          auto* wc =
                              AsInstrumentedWebContents(el)->web_contents();
                          EXPECT_EQ(true, content::EvalJs(wc, script));
                        },
                        check_message_script)));
  } else {
    // If AimTriggeredThreadLinks is disabled, the window.open call moves
    // Contextual Tasks into the side panel. Therefore, instrument the side
    // panel <webview> and ensure the postMessage was received.
    sequence = Steps(
        std::move(sequence),
        WaitForShow(kContextualTasksSidePanelWebViewElementId),
        NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                         "SidePanelContentWebViewName",
                         [](ContextualTasksWebView* web_view) -> views::View* {
                           return web_view->content_web_view();
                         }),
        InstrumentNonTabWebView(kPrimaryTab2, "SidePanelContentWebViewName"),
        InstrumentInnerWebContents(kInnerWebContentsId2, kPrimaryTab2, 0),
        WaitForShow(kInnerWebContentsId2),
        WithElement(kInnerWebContentsId2,
                    base::BindOnce(
                        [](std::string script, ui::TrackedElement* el) {
                          auto* wc =
                              AsInstrumentedWebContents(el)->web_contents();
                          EXPECT_EQ(true, content::EvalJs(wc, script));
                        },
                        check_message_script)));
  }

  RunTestSequence(InAnyContext(std::move(sequence)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksInteractiveUiTestParameterized,
    testing::Combine(testing::Values(UserVariation::kSignedIn,
                                     UserVariation::kSignedOut,
                                     UserVariation::kIncognito),
                     testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<UserVariation, bool>>& info) {
      return base::StrCat(
          {UserVariationToString(testing::TestParamInfo<UserVariation>(
               std::get<0>(info.param), info.index)),
           std::get<1>(info.param) ? "_TriggeredThreadLinks"
                                   : "_NoTriggeredThreadLinks"});
    });

class ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled
    : public ContextualTasksInteractiveUiTest {
 public:
  ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled() =
      default;
  ~ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled()
      override = default;

  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    auto disabled = GetDefaultDisabledFeatures();
    disabled.push_back(lens::features::kLensOnlySendAaiForModalityChips);
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }
};

IN_PROC_BROWSER_TEST_P(
    ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled,
    AddAndSubmitTabFromComposebox_SendsAai) {
  SkipIfIncognito("Tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      NavigateWebContents(kPrimaryTab, GURL(chrome::kChromeUIVersionURL)),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl),
      WaitForWebContentsReady(kGenericTab, kGenericPageUrl),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, 0),
      WaitForFaviconGroupWithTitle(kSidePanelId, "title1.html"),
      WaitForComposeboxFilesCount(kSidePanelId, 1),
      WaitForSubmitButtonEnabled(kSidePanelId),
      ClickButton(kSidePanelId, kSubmitButton),
      // When flag is disabled, unresolved URLs (Tabs) are sent in AAI.
      VerifySubmitQueryMessage(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          "title1.html"));
}

IN_PROC_BROWSER_TEST_P(
    ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled,
    AddAndSubmitPdfChipFromComposebox_SendsAai) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath file_path = test_data_dir.AppendASCII("download.pdf");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, "fileUpload"),
      WaitForDocumentChipWithTitle(kSidePanelId, "download.pdf"),
      WaitForComposeboxFilesCount(kSidePanelId, 1),
      WaitForSubmitButtonEnabled(kSidePanelId),
      ClickButton(kSidePanelId, kSubmitButton),
      // When flag is disabled, PDF is sent in AAI.
      VerifySubmitQueryMessage(lens::LensOverlayRequestId::MEDIA_TYPE_PDF,
                               "download.pdf"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksInteractiveUiTestWithAaiOnlyForModalityChipsDisabled,
    testing::Values(UserVariation::kSignedIn,
                    UserVariation::kSignedOut,
                    UserVariation::kIncognito),
    &UserVariationToString);

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       RecontextualizationViewportChangeOnly) {
  SkipIfIncognito(
      "Auto-suggested tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  const int expected_turn2_viewport_image_count = 0;
  uint64_t session_uuid = 0;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for WebUI components to load.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      // Navigate active tab to a valid page.
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Turn 1: The tab context is auto-suggested and submitted.
      WaitForFaviconGroupWithTitle(kSidePanelWebContentsId, "title1.html"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify Turn 1 query has the webpage and image context.
      VerifySubmitQueryMessageAndExtractUuid(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          std::nullopt, /*expected_message_index=*/0, std::nullopt,
          std::ref(session_uuid)),

      // Turn 2: Submit a second query without changing the viewport screenshot.
      InputText(kSidePanelWebContentsId, "second query"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify Turn 2 query is sent, but since the viewport did not change,
      // no new context upload occurred (0 uploads).
      VerifyMultipleSubmitQueryMessage("second query",
                                       expected_turn2_viewport_image_count,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{},
                                       /*expected_message_index=*/1),

      // Turn 3: Change the viewport screenshot color (simulates
      // scrolling/viewport change).
      Do([&]() {
        TestTabContextualizationController::screenshot_color_ = SK_ColorBLUE;
      }),

      // Submit a third query.
      InputText(kSidePanelWebContentsId, "third query"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),

      // Verify Turn 3 query is sent, and because the screenshot changed,
      // a new context upload was triggered, producing a webpage and image
      // context.
      VerifySubmitQueryMessageWithExpectedUuid(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          std::nullopt, /*expected_message_index=*/2, std::nullopt,
          std::ref(session_uuid)));
}

class ContextualTasksRecontextUiTest
    : public ContextualTasksInteractiveUiTestBase,
      public testing::WithParamInterface<std::tuple<UserVariation, bool>> {
 public:
  ContextualTasksRecontextUiTest() = default;
  ~ContextualTasksRecontextUiTest() override = default;

  bool GetSidePanelParam() const { return std::get<1>(GetParam()); }

  void SetUpFeatureList() override {
    auto disabled = GetDefaultDisabledFeatures();
    if (GetSidePanelParam()) {
      std::vector<base::test::FeatureRefAndParams> enabled;
      for (const auto& f : GetDefaultEnabledFeatures()) {
        if (&f.feature.get() != &kContextualTasks) {
          enabled.push_back(f);
        }
      }
      disabled.push_back(kContextualTasks);
      enabled.push_back({kContextualTasksSidePanel, {}});
      feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
    } else {
      auto enabled = GetDefaultEnabledFeatures();
      disabled.push_back(kContextualTasksSidePanel);
      feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
    }
  }

 protected:
  UserVariation GetUserVariation() const override {
    return std::get<0>(GetParam());
  }
};

IN_PROC_BROWSER_TEST_P(ContextualTasksRecontextUiTest,
                       RecontextualizationAfterContextualSearchboxQuery) {
  SkipIfIncognito("Tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  const DeepQuery kPathToComposeboxInput = {"contextual-tasks-app",
                                            "#composebox", "#composebox",
                                            "#composeboxInput", "#input"};

  const DeepQuery kPathToOverlaySearchboxInput{
      "lens-overlay-app",
      "cr-lens-searchbox",
      "cr-searchbox-input",
      "input",
  };

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  uint64_t session_uuid = 0;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),

      // Navigate active tab to a valid page before opening Lens overlay.
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),
      WaitForWebContentsPainted(kPrimaryTab),

      // Trigger Lens Overlay.
      Do([&]() {
        LensSearchController* lens_search_controller =
            LensSearchController::FromTabWebContents(
                browser()->tab_strip_model()->GetActiveWebContents());
        lens_search_controller->OpenLensOverlay(
            lens::LensOverlayInvocationSource::kAppMenu);
      }),

      // Wait for the overlay to load.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),

      // Turn 1: Issue a query from the Lens Overlay Contextual Searchbox entry
      // point.
      InSameContext(
          WaitForShow(LensOverlayController::kOverlayId), Do([&]() {
            content::WebContents* web_contents =
                browser()->tab_strip_model()->GetActiveWebContents();
            auto* lens_controller =
                LensSearchController::FromTabWebContents(web_contents);
            auto* query_router = static_cast<lens::FakeLensQueryFlowRouter*>(
                lens_controller->query_router());
            CHECK(query_router);
            auto* session_handle = static_cast<
                contextual_search::MockContextualSearchSessionHandle*>(
                query_router->GetContextualSearchSessionHandle());
            CHECK(session_handle);
            ON_CALL(*session_handle,
                    CreateSearchUrl(::testing::_, ::testing::_))
                .WillByDefault(::testing::WithArg<1>([](base::OnceCallback<void(
                                                            GURL)> callback) {
                  std::move(callback).Run(GURL(
                      "https://www.google.com/search?q=first+query&udm=50"));
                }));
          }),
          WaitForElementExists(kOverlayId, kPathToOverlaySearchboxInput),
          FocusWebContents(kOverlayId),
          ExecuteJsAt(kOverlayId, kPathToOverlaySearchboxInput,
                      "(el) => { el.focus(); }",
                      ExecuteJsMode::kWaitForCompletion),
          ExecuteJsAt(
              kOverlayId, kPathToOverlaySearchboxInput,
              "(el) => { el.value = 'first query'; el.dispatchEvent(new "
              "Event('input', { bubbles: true })); el.dispatchEvent(new "
              "Event('change', { bubbles: true }));}",
              ExecuteJsMode::kWaitForCompletion),
          ExecuteJsAt(kOverlayId, kPathToOverlaySearchboxInput,
                      "(el) => { el.dispatchEvent(new KeyboardEvent('keydown', "
                      "{ key:'Enter', bubbles: true, cancelable: true, "
                      "composed: true }));}",
                      ExecuteJsMode::kFireAndForget)),

      // The query should transition to the Contextual Tasks Side Panel.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // Wait for WebUI components to load in the Side Panel.
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),

      // Turn 2: Change the viewport screenshot color (simulates
      // scrolling/viewport change).
      Do([&]() {
        TestTabContextualizationController::screenshot_color_ = SK_ColorBLUE;
      }),

      // Emulate focus into the searchbox. Turn 2: submit a new query to force
      // re-upload.
      FocusWebContents(kSidePanelWebContentsId),
      ExecuteJsAt(kSidePanelWebContentsId,
                  DeepQuery{"contextual-tasks-app", "#composebox",
                            "#composebox", "#composeboxInput", "#input"},
                  R"(el => {
                      if (el.tagName === 'TEXTAREA') {
                          el.value = 'second query';
                      } else {
                          el.innerText = 'second query';
                      }
                      el.dispatchEvent(new Event('input', { bubbles: true, composed: true }));
                      el.dispatchEvent(new Event('change', { bubbles: true, composed: true }));
                  })"),
      ExecuteJsAt(
          kSidePanelWebContentsId,
          DeepQuery{"contextual-tasks-app", "#composebox", "#composebox",
                    "cr-composebox-submit", "#submitContainer"},
          R"(el => { el.click(); })", ExecuteJsMode::kFireAndForget),

      // Verify Turn 2 query correctly triggers an image upload due to the
      // viewport change.
      VerifySubmitQueryMessageAndExtractUuid(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          std::nullopt, /*expected_message_index=*/0, std::nullopt,
          std::ref(session_uuid)),

      // Turn 3: Change the viewport again.
      Do([&]() {
        TestTabContextualizationController::screenshot_color_ = SK_ColorRED;
      }),

      // Emulate focus into the searchbox. Turn 3: submit a third query.
      FocusWebContents(kSidePanelWebContentsId),
      ExecuteJsAt(kSidePanelWebContentsId,
                  DeepQuery{"contextual-tasks-app", "#composebox",
                            "#composebox", "#composeboxInput", "#input"},
                  R"(el => {
                      if (el.tagName === 'TEXTAREA') {
                          el.value = 'third query';
                      } else {
                          el.innerText = 'third query';
                      }
                      el.dispatchEvent(new Event('input', { bubbles: true, composed: true }));
                      el.dispatchEvent(new Event('change', { bubbles: true, composed: true }));
                  })"),
      ExecuteJsAt(
          kSidePanelWebContentsId,
          DeepQuery{"contextual-tasks-app", "#composebox", "#composebox",
                    "cr-composebox-submit", "#submitContainer"},
          R"(el => { el.click(); })", ExecuteJsMode::kFireAndForget),

      // Verify Turn 3 query correctly triggers an image upload and maintains
      // the UUID.
      VerifySubmitQueryMessageWithExpectedUuid(
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
          std::nullopt, /*expected_message_index=*/1, std::nullopt,
          std::ref(session_uuid)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksRecontextUiTest,
    testing::Combine(testing::Values(UserVariation::kSignedIn,
                                     UserVariation::kSignedOut,
                                     UserVariation::kIncognito),
                     testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<UserVariation, bool>>& info) {
      return base::StrCat(
          {UserVariationToString(testing::TestParamInfo<UserVariation>(
               std::get<0>(info.param), info.index)),
           std::get<1>(info.param) ? "_SidePanelOnly" : "_ContextualTasks"});
    });

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       QueryOpenAndCloseFlow) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),

      InputText(kSidePanelId, "How to make kombucha?"),
      SubmitQueryViaSubmitButton(kSidePanelId),
      VerifyMultipleSubmitQueryMessage("How to make kombucha?",
                                       /*expected_viewport_image_count=*/0,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{}),
      WaitForInputCleared(kSidePanelId),

      CloseContextualTasksSidePanel());
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       NewTaskThreadInteraction) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  RunTestSequence(InstrumentTab(kPrimaryTab, 0),
                  SelectTab(kTabStripElementId, 0),
                  OpenContextualTasksInSidePanel(kSidePanelId),

                  InputText(kSidePanelId, "some draft text"),
                  WaitForInputValue(kSidePanelId, "some draft text"),
                  ClickNewThreadButton(kSidePanelId),
                  // New thread navigates the embedded thread frame, so tolerate
                  // navigation while waiting for the composebox input to clear.
                  WaitForInputCleared(kSidePanelId,
                                      /*continue_across_navigation=*/true));
}

// Doesn't click #voiceSearchButton (would invoke
// webkitSpeechRecognition.start).
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       IntegrationVoiceLightweight) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  const DeepQuery kVoiceButtonPath = {"contextual-tasks-app", "#composebox",
                                      "#composebox", "#voiceSearchButton"};
  const DeepQuery kVoiceSearchComponentPath = {
      "contextual-tasks-app", "#composebox", "#composebox", "#voiceSearch"};
  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),

      EnsureVoiceSearchAvailable(kSidePanelId),
      WaitForElementExists(kSidePanelId, kVoiceButtonPath),
      WaitForElementExists(kSidePanelId, kVoiceSearchComponentPath),

      DispatchVoiceSearchFinalResult(kSidePanelId, "test query"),
      VerifyMultipleSubmitQueryMessage("test query",
                                       /*expected_viewport_image_count=*/0,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{}));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       ClickSuggestionSubmitsQuery) {
  SkipIfIncognito("Search suggestions are disabled in Incognito mode.");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  // In the side panel, typed matches render in
  // #contextualTasksSuggestionsContainer, not the composebox's internal
  // #matches.
  const DeepQuery kSuggestionMatchText = {
      "contextual-tasks-app", "#composebox",
      "#contextualTasksSuggestionsContainer", "#match1", "#textContainer"};
  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),

      InputText(kSidePanelId, "kombucha"),
      // Wait for the suggestion element to exist before checking its text,
      // avoiding a JS null vs string type mismatch during initial polling.
      WaitForElementExists(kSidePanelId, kSuggestionMatchText),
      WaitForJsResultAt(kSidePanelId, kSuggestionMatchText,
                        "el => el.textContent.trim()",
                        std::string("suggestion-1")),
      ClickButton(kSidePanelId, kSuggestionMatchText),
      VerifyMultipleSubmitQueryMessage("suggestion-1",
                                       /*expected_viewport_image_count=*/0,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{}));
}

// CUJ covered by this test:
// 1) User navigates to google search contextual tasks trigger URL (udm=50)
// 2) The tab should navigate to chrome://contextual-tasks
// 3) The composebox becomes visible and the input field is focused
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       FocusComposeboxOnInitialLoad) {
  // TODO(crbug.com/533072196): Clean up this test once the pre-rearchitecture
  // logic is cleaned up.
  SkipIfNotSignedIn(
      "Top-level Contextual Tasks tab navigation interception requires a "
      "signed-in browser session.");

  const GURL kInterceptionUrl("https://www.google.com/search?udm=50");

  StateChange composebox_focused;
  composebox_focused.type = StateChange::Type::kExistsAndConditionTrue;
  composebox_focused.where = {"contextual-tasks-app"};
  composebox_focused.test_function =
      "function(app) {"
      "  const cb = app?.shadowRoot?.querySelector('#composebox');"
      "  const crCb = cb?.shadowRoot?.querySelector('#composebox');"
      "  const inputSuite = "
      "crCb?.shadowRoot?.querySelector('#composeboxInput');"
      "  const input = inputSuite?.shadowRoot?.querySelector('#input');"
      "  return !app.isComposeboxHidden_() && input && "
      "         input.getRootNode().activeElement === input;"
      "}";
  composebox_focused.event = kElementExistsEvent;

  RunTestSequence(InstrumentTab(kPrimaryTab, 0),
                  SelectTab(kTabStripElementId, 0),
                  OpenContextualTasksInCurrentTab(kInterceptionUrl),
                  WaitForStateChange(kPrimaryTab, composebox_focused));
}

// CUJ covered by this test:
// 1) Opens Contextual Tasks in a tab.
// 2) Call window.open from the Contextual Tasks <webview>.
// 3) The window opens in a new tab.
// 4) In the <webview>, call location.href on the opened window.
// 5) Verify the opened tab navigates to the new URL.
// 6) Verify the original contextual tasks tab is still there.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTestParameterized,
                       LocationHrefRedirectedToOpenedTab) {
  // TODO(crbug.com/533072196): Clean up this test once the pre-rearchitecture
  // logic is cleaned up.
  SkipIfNotSignedIn(
      "Webview window.open and tab-strip opener routing tests require "
      "Contextual Tasks in a top-level browser tab, which requires a "
      "signed-in session.");

  if (!GetAimTriggeredThreadLinksParam()) {
    GTEST_SKIP()
        << "Requires AimTriggeredThreadLinks (window tracking) enabled";
  }

  const GURL kActiveTabUrl =
      https_server_.GetURL("myaccount.google.com", "/title1.html");
  const GURL initial_url =
      https_server_.GetURL("myaccount.google.com", "/title1.html#citation");
  const GURL new_url =
      https_server_.GetURL("myaccount.google.com", "/title2.html");
  const GURL kInterceptionUrl =
      https_server_.GetURL(kMockAimPageHost, "/search?udm=50");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOpenedTab);

  // Step 1: Open Contextual Tasks in a Tab
  auto sequence =
      Steps(InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
            OpenContextualTasksInCurrentTab(kInterceptionUrl),
            InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0));

  sequence = Steps(
      std::move(sequence),
      // 1. Set up instrumenting the next tab BEFORE we trigger window.open to
      // avoid race conditions.
      InstrumentNextTab(kOpenedTab),

      // 2. Within the <webview>, open a window and store its reference.
      WithElement(kInnerWebContentsId,
                  base::BindOnce(
                      [](GURL url, ui::TrackedElement* el) {
                        auto* wc =
                            AsInstrumentedWebContents(el)->web_contents();
                        std::string open_script = content::JsReplace(
                            R"(
                  (() => {
                    window.openedWindow = window.open($1, '_blank');
                  })();
                )",
                            url.spec());
                        EXPECT_TRUE(content::ExecJs(wc, open_script));
                      },
                      initial_url)),

      // 3. Wait for the opened tab to finish loading the initial URL.
      WaitForWebContentsNavigation(kOpenedTab, initial_url),

      // 4. Within the <webview>, set location.href on the opened window.
      WithElement(kInnerWebContentsId,
                  base::BindOnce(
                      [](GURL url, ui::TrackedElement* el) {
                        auto* wc =
                            AsInstrumentedWebContents(el)->web_contents();
                        std::string navigate_script = content::JsReplace(
                            R"(
                  (() => {
                    if (!window.openedWindow) {
                      return "no opened window";
                    }
                    window.openedWindow.location.href = $1;
                    return "ok";
                  })();
                )",
                            url.spec());
                        EXPECT_EQ("ok", content::EvalJs(wc, navigate_script));
                      },
                      new_url)),

      // 5. Verify the opened tab navigates to the new URL.
      WaitForWebContentsNavigation(kOpenedTab, new_url),

      // 6. Switch back to the original tab and verify it's still there.
      SelectTab(kTabStripElementId, 0), WaitForShow(kInnerWebContentsId));

  RunTestSequence(std::move(sequence));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       ComposeboxLensButtonIsEnabled) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  // #lensIcon is in the inner cr-composebox nested under the contextual tasks
  // composebox, hence the doubled #composebox.
  const DeepQuery kLensIcon = {"contextual-tasks-app", "#composebox",
                               "#composebox", "#lensIcon"};
  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      WaitForElementExists(kSidePanelId, kLensIcon),
      WaitForJsResultAt(kSidePanelId, kLensIcon, "el => !el.disabled", true));
}

class ContextualTasksCopyUrlTest
    : public ContextualTasksInteractiveUiTestBase,
      public testing::WithParamInterface<std::tuple<UserVariation, bool>> {
 public:
  ContextualTasksCopyUrlTest() = default;
  ~ContextualTasksCopyUrlTest() override = default;

  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    auto disabled = GetDefaultDisabledFeatures();
    std::vector<base::test::FeatureRef> webui_features = {
        features::kInitialWebUI, features::kWebUILocationBar,
        omnibox::internal::kWebUIOmniboxAimPopup};
    if (IsWebUI()) {
      std::erase(disabled, omnibox::internal::kWebUIOmniboxAimPopup);
      for (const auto& f : webui_features) {
        enabled.push_back({*f, {}});
      }
    } else {
      for (const auto& f : webui_features) {
        disabled.push_back(f);
      }
    }
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  bool IsWebUI() const { return std::get<1>(GetParam()); }

  views::WebView* GetWebUIToolbarWebView() {
    return ::GetWebUIToolbarWebView(browser())->GetWebViewForTesting();
  }

  ui::test::InteractiveTestApi::MultiStep SetupTest() {
    if (IsWebUI()) {
      return Steps(
          InstrumentTab(kPrimaryTab),
          InstrumentNonTabWebView(kWebUIToolbarId, GetWebUIToolbarWebView(),
                                  /*wait_for_ready=*/true));
    } else {
      return Steps(InstrumentTab(kPrimaryTab));
    }
  }

  ui::test::InteractiveTestApi::MultiStep FocusAndCopy(
      ui::Accelerator focus_accelerator,
      ui::Accelerator copy_accelerator) {
    const WebContentsInteractionTestUtil::DeepQuery kTextInputDeepQuery = {
        "toolbar-app", "location-bar", "readonly-omnibox", "#textInput",
        "#input"};
    const WebContentsInteractionTestUtil::DeepQuery kReadonlyOmniboxDeepQuery =
        {"toolbar-app", "location-bar", "readonly-omnibox"};
    if (IsWebUI()) {
      return Steps(FocusWebContents(kPrimaryTab),
                   SendAccelerator(kBrowserViewElementId, focus_accelerator),
                   WaitForJsResultAt(kWebUIToolbarId, kTextInputDeepQuery,
                                     "el => el.matches(':focus-visible')"),
                   ExecuteJsAt(kWebUIToolbarId, kTextInputDeepQuery,
                               "(el) => { el.select(); }"),
                   WaitForJsResultAt(kWebUIToolbarId, kReadonlyOmniboxDeepQuery,
                                     "el => el.adjustedCopyResult_ !== null"),
                   ExecuteJsAt(kWebUIToolbarId, kTextInputDeepQuery,
                               "(el) => { document.execCommand('copy'); }"));
    } else {
      return Steps(FocusWebContents(kPrimaryTab),
                   SendAccelerator(kBrowserViewElementId, focus_accelerator),
                   SendAccelerator(kOmniboxElementId, copy_accelerator));
    }
  }

 protected:
  UserVariation GetUserVariation() const override {
    return std::get<0>(GetParam());
  }
};

// TODO(crbug.com/542608217): Disable on Linux MSan due to failure/flakiness.
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_CopyUrl DISABLED_CopyUrl
#else
#define MAYBE_CopyUrl CopyUrl
#endif
IN_PROC_BROWSER_TEST_P(ContextualTasksCopyUrlTest, MAYBE_CopyUrl) {
  // TODO(crbug.com/533072196): Clean up this test once the pre-rearchitecture
  // logic is cleaned up.
  SkipIfNotSignedIn(
      "Testing LocationBar URL swapping requires Contextual Tasks in a "
      "top-level browser tab, which requires a signed-in session.");

  content::BrowserTestClipboardScope test_clipboard_scope;
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50&q=test");

  ui::Accelerator focus_accelerator;
  ui::Accelerator copy_accelerator;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view->GetAcceleratorForCommandId(IDC_FOCUS_LOCATION,
                                                       &focus_accelerator));
  ASSERT_TRUE(
      browser_view->GetAcceleratorForCommandId(IDC_COPY, &copy_accelerator));

  RunTestSequence(
      SetupTest(), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),

      // Instrument the inner WebContents and wait for it to load.
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      FocusAndCopy(focus_accelerator, copy_accelerator),

      // Verify clipboard.
      ObserveState(kClipboardText,
                   []() { return ui::ClipboardMonitor::GetInstance(); }),
      // The display URL is chrome://google.com/search?q=test&udm=50 (with
      // chrome:// scheme) but when copied it should be swapped to
      // https://www.google.com/search?udm=50&q=test...
      WaitForState(
          kClipboardText,
          testing::ResultOf(
              [](const std::u16string& s) { return base::UTF16ToUTF8(s); },
              testing::StartsWith(
                  "https://www.google.com/search?udm=50&q=test"))),
      StopObservingState(kClipboardText),
      UninstrumentWebContents(kInnerWebContentsId,
                              /*fail_if_not_instrumented=*/false));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksCopyUrlTest, FocusAndBlur) {
  // TODO(crbug.com/533072196): Clean up this test once the pre-rearchitecture
  // logic is cleaned up.
  SkipIfNotSignedIn(
      "Testing LocationBar omnibox focus/blur requires Contextual Tasks in a "
      "top-level browser tab, which requires a signed-in session.");

  const bool is_webui = IsWebUI();

#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
  if (is_webui) {
    // TODO(crbug.com/546594688): Re-enable test
    GTEST_SKIP() << "Disabled on Linux MSan for WebUI because test flakiness";
  }
#endif
  const GURL kInterceptionUrl("https://www.google.com/search?udm=50&q=test");

  ui::Accelerator focus_accelerator;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view->GetAcceleratorForCommandId(IDC_FOCUS_LOCATION,
                                                       &focus_accelerator));

  const WebContentsInteractionTestUtil::DeepQuery kTextInputDeepQuery = {
      "toolbar-app", "location-bar", "readonly-omnibox", "#textInput",
      "#input"};

  auto focus_omnibox = [this, focus_accelerator, kTextInputDeepQuery,
                        is_webui]() {
    if (is_webui) {
      // WebUI
      return Steps(FocusWebContents(kPrimaryTab),
                   SendAccelerator(kBrowserViewElementId, focus_accelerator),
                   WaitForJsResultAt(kWebUIToolbarId, kTextInputDeepQuery,
                                     "el => el.matches(':focus-visible')"));
    } else {
      // Views
      return Steps(FocusWebContents(kPrimaryTab),
                   FocusElement(kOmniboxElementId));
    }
  };

  auto get_omnibox_state = [browser_view](bool* in_progress,
                                          std::u16string* text) {
    return [browser_view, in_progress, text]() {
      LocationBar* lb = browser_view->GetLocationBar();
      if (in_progress) {
        *in_progress =
            lb->GetOmniboxController()->edit_model()->user_input_in_progress();
      }
      if (text) {
        *text = lb->GetOmniboxView()->GetText();
      }
    };
  };

  bool in_progress = false;
  std::u16string initial_text;
  std::u16string modified_text;

  RunTestSequence(
      SetupTest(), SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInCurrentTab(kInterceptionUrl),

      // Instrument the inner WebContents and wait for it to load.
      InstrumentInnerWebContents(kInnerWebContentsId, kPrimaryTab, 0),

      // 1. Focus the omnibox.
      focus_omnibox(),

      // 2. Verify user_input_in_progress is false.
      Do(get_omnibox_state(&in_progress, &initial_text)),
      Check([&in_progress]() { return !in_progress; },
            "Verify user_input_in_progress is false initially"),

      // 3. Type "a".
      If([is_webui]() { return is_webui; },
         Then(SendKeyPress(kWebUIToolbarId, ui::VKEY_A)),
         Else(SendKeyPress(kOmniboxElementId, ui::VKEY_A))),

      // For WebUI, wait for the JS value to update to "a".
      If([is_webui]() { return is_webui; },
         Then(WaitForJsResultAt(kWebUIToolbarId, kTextInputDeepQuery,
                                "el => el.value === 'a'"))),

      // 4. Verify user_input_in_progress is true, and text is "a".
      Do(get_omnibox_state(&in_progress, &modified_text)),
      Check([&in_progress]() { return in_progress; },
            "Verify user_input_in_progress is true after typing"),
      Check([&modified_text]() { return modified_text == u"a"; },
            "Verify text is 'a' after typing"),

      // 5. Blur the omnibox.
      FocusWebContents(kPrimaryTab),

      // 6. Verify user_input_in_progress is still true, and text is still "a"
      // (not reverted).
      Do(get_omnibox_state(&in_progress, &modified_text)),
      Check([&in_progress]() { return in_progress; },
            "Verify user_input_in_progress is still true after blur"),
      Check([&modified_text]() { return modified_text == u"a"; },
            "Verify text is still 'a' after blur"),

      // 7. Focus it again, and press ESC to revert.
      focus_omnibox(),
      If([is_webui]() { return is_webui; },
         Then(SendKeyPress(kWebUIToolbarId, ui::VKEY_ESCAPE)),
         Else(SendKeyPress(kOmniboxElementId, ui::VKEY_ESCAPE))),

      // For WebUI, wait for the JS value to revert (not be "a").
      If([is_webui]() { return is_webui; },
         Then(WaitForJsResultAt(kWebUIToolbarId, kTextInputDeepQuery,
                                "el => el.value !== 'a'"))),

      // 8. Verify user_input_in_progress is false, and text is reverted to
      // initial.
      Do(get_omnibox_state(&in_progress, &modified_text)),
      Check([&in_progress]() { return !in_progress; },
            "Verify user_input_in_progress is false after revert"),
      Check([&modified_text,
             &initial_text]() { return modified_text == initial_text; },
            "Verify text is reverted after revert"),
      UninstrumentWebContents(kInnerWebContentsId,
                              /*fail_if_not_instrumented=*/false));
}

// This tests the following CUJ:
//  (1) User navigates to a webpage.
//  (2) User opens the Chrome app menu and selects "Search with Google Lens".
//  (3) Lens overlay opens over the page.
//  (4) User drags to select a region of the page in the Lens overlay.
//  (5) Contextual Tasks side panel opens with a task for the tab and loads
//      the Lens region search results.
//  (6) Lens overlay stays open and keeps showing the selected region.
IN_PROC_BROWSER_TEST_P(ContextualTasksInteractiveUiTest,
                       LensOverlayRegionSearchHandoff) {
  UseRealLensQueryFlowRouter();
  WaitForTemplateURLServiceToLoad();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kThreadFrame = {"contextual-tasks-app", "#threadFrame"};
  const DeepQuery kSelectionOverlay = {"lens-overlay-app",
                                       "lens-selection-overlay"};

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->GetBoundsInScreen().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  auto get_lens_controller = [this]() {
    return LensSearchController::FromTabWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  };

  RunTestSequence(
      // (1)-(2) Navigate to a page and select Lens from the app menu.
      OpenLensOverlay(),

      // (3) The Lens overlay opens from the app menu entry point.
      InAnyContext(WaitForShow(LensOverlayController::kOverlayId)),
      CheckResult(
          [get_lens_controller]() {
            return get_lens_controller()->invocation_source();
          },
          std::make_optional(lens::LensOverlayInvocationSource::kAppMenu)),

      // (4) Drag to select a region in the overlay.
      SelectRegionInLensOverlay(kOverlayId, std::move(off_center_point)),

      // (5) The region query opens the Contextual Tasks side panel for a task
      // associated with the tab, and the panel loads the region results URL.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      Check(
          [this]() {
            return ContextualTasksPanelController::From(browser())
                ->IsPanelOpenForContextualTask();
          },
          "Side panel is open for a contextual task"),
      Check(
          [this]() {
            return CHECK_DEREF(ContextualTasksServiceFactory::GetForProfile(
                                   browser()->GetProfile()))
                .GetContextualTaskForTab(sessions::SessionTabHelper::IdForTab(
                    browser()->tab_strip_model()->GetActiveWebContents()))
                .has_value();
          },
          "Active tab is associated with a contextual task"),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "LensRegionHandoffSidePanelContents",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelId,
                              "LensRegionHandoffSidePanelContents"),
      WaitForElementExists(kSidePanelId, kThreadFrame),
      WaitForJsResultAt(kSidePanelId, kThreadFrame,
                        "el => {"
                        "  if (!el.src || !URL.canParse(el.src)) return false;"
                        "  const url = new URL(el.src);"
                        "  return url.origin === 'https://www.google.com' &&"
                        "         url.pathname === '/search' &&"
                        "         url.searchParams.get('udm') === '26' &&"
                        "         url.searchParams.get('lns_mode') === 'un' &&"
                        "         url.searchParams.get('lns_fp') === '1' &&"
                        "         Boolean(url.searchParams.get('vsrid')) &&"
                        "         Boolean(url.searchParams.get('vsint')) &&"
                        "         Boolean(url.searchParams.get('gsessionid'));"
                        "}"),

      // (6) The overlay stays open with the region still selected.
      CheckResult(
          [get_lens_controller]() {
            return get_lens_controller()
                ->lens_overlay_controller()
                ->IsOverlayShowing();
          },
          true),
      InAnyContext(WaitForJsResultAt(
          kOverlayId, kSelectionOverlay,
          "el => !!el.shadowRoot?.querySelector('#postSelectionRenderer')"
          "?.hasSelection?.()")),
      CheckResult(
          [get_lens_controller]() {
            return get_lens_controller()
                ->lens_overlay_controller()
                ->HasRegionSelection();
          },
          true));
}

class ContextualTasksSeparateViewportRequestIdsInteractiveUiTest
    : public ContextualTasksInteractiveUiTest {
 public:
  ContextualTasksSeparateViewportRequestIdsInteractiveUiTest() = default;
  ~ContextualTasksSeparateViewportRequestIdsInteractiveUiTest() override =
      default;

  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    auto disabled = GetDefaultDisabledFeatures();
    enabled.push_back(
        {lens::features::kLensUseSeparateRequestIdForViewportImages, {}});
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }
};

IN_PROC_BROWSER_TEST_P(
    ContextualTasksSeparateViewportRequestIdsInteractiveUiTest,
    LensOverlayRegionSearchHandoff) {
  UseRealLensQueryFlowRouter();
  WaitForTemplateURLServiceToLoad();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kThreadFrame = {"contextual-tasks-app", "#threadFrame"};
  const DeepQuery kSelectionOverlay = {"lens-overlay-app",
                                       "lens-selection-overlay"};

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->GetBoundsInScreen().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  auto get_lens_controller = [this]() {
    return LensSearchController::FromTabWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  };

  RunTestSequence(
      // (1)-(2) Navigate to a page and select Lens from the app menu.
      OpenLensOverlay(),

      // (3) The Lens overlay opens from the app menu entry point.
      InAnyContext(WaitForShow(LensOverlayController::kOverlayId)),
      CheckResult(
          [get_lens_controller]() {
            return get_lens_controller()->invocation_source();
          },
          std::make_optional(lens::LensOverlayInvocationSource::kAppMenu)),

      // (4) Drag to select a region in the overlay.
      SelectRegionInLensOverlay(kOverlayId, std::move(off_center_point)),

      // (5) The region query opens the Contextual Tasks side panel for a task
      // associated with the tab, and the panel loads the region results URL.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      Check(
          [this]() {
            return ContextualTasksPanelController::From(browser())
                ->IsPanelOpenForContextualTask();
          },
          "Side panel is open for a contextual task"),
      Check(
          [this]() {
            return CHECK_DEREF(ContextualTasksServiceFactory::GetForProfile(
                                   browser()->GetProfile()))
                .GetContextualTaskForTab(sessions::SessionTabHelper::IdForTab(
                    browser()->tab_strip_model()->GetActiveWebContents()))
                .has_value();
          },
          "Active tab is associated with a contextual task"),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "LensRegionHandoffSidePanelContents",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelId,
                              "LensRegionHandoffSidePanelContents"),
      WaitForElementExists(kSidePanelId, kThreadFrame),
      WaitForJsResultAt(kSidePanelId, kThreadFrame,
                        "el => {"
                        "  if (!el.src || !URL.canParse(el.src)) return false;"
                        "  const url = new URL(el.src);"
                        "  return url.origin === 'https://www.google.com' &&"
                        "         url.pathname === '/search' &&"
                        "         url.searchParams.get('udm') === '26' &&"
                        "         url.searchParams.get('lns_mode') === 'un' &&"
                        "         url.searchParams.get('lns_fp') === '1' &&"
                        "         Boolean(url.searchParams.get('cinpts')) &&"
                        "         !url.searchParams.has('vsrid') &&"
                        "         Boolean(url.searchParams.get('vsint')) &&"
                        "         Boolean(url.searchParams.get('gsessionid'));"
                        "}"),
      CheckJsResultAt(
          kSidePanelId, kThreadFrame,
          "el => new URL(el.src).searchParams.get('cinpts')",
          testing::Truly([](const std::string& cinpts_param) {
            std::string serialized_proto;
            if (!base::Base64UrlDecode(
                    cinpts_param, base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                    &serialized_proto)) {
              return false;
            }
            lens::LensOverlayContextualInputs proto;
            if (!proto.ParseFromString(serialized_proto)) {
              return false;
            }
            if (proto.inputs_size() != 2) {
              return false;
            }
            bool viewport_is_first = proto.inputs(0).is_viewport_image();
            const auto& viewport_input =
                proto.inputs(viewport_is_first ? 0 : 1);
            const auto& page_input = proto.inputs(viewport_is_first ? 1 : 0);
            return viewport_input.is_viewport_image() &&
                   viewport_input.request_id().media_type() ==
                       lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE &&
                   !page_input.is_viewport_image() &&
                   page_input.request_id().media_type() ==
                       lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE &&
                   viewport_input.request_id().context_id() ==
                       page_input.request_id().context_id() &&
                   viewport_input.request_id().uuid() !=
                       page_input.request_id().uuid();
          })),

      // (6) The overlay stays open with the region still selected.
      CheckResult(
          [get_lens_controller]() {
            return get_lens_controller()
                ->lens_overlay_controller()
                ->IsOverlayShowing();
          },
          true),
      InAnyContext(WaitForJsResultAt(
          kOverlayId, kSelectionOverlay,
          "el => !!el.shadowRoot?.querySelector('#postSelectionRenderer')"
          "?.hasSelection?.()")),
      CheckResult(
          [get_lens_controller]() {
            return get_lens_controller()
                ->lens_overlay_controller()
                ->HasRegionSelection();
          },
          true));
}

IN_PROC_BROWSER_TEST_P(
    ContextualTasksSeparateViewportRequestIdsInteractiveUiTest,
    AddAndSubmitTabFromComposebox) {
  SkipIfIncognito("Tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);

  const DeepQuery kFaviconGroup = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "composebox-favicon-group"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0),
      NavigateWebContents(kPrimaryTab, GURL(chrome::kChromeUIVersionURL)),
      AddInstrumentedTab(kGenericTab, kGenericPageUrl),
      WaitForWebContentsReady(kGenericTab, kGenericPageUrl),
      SelectTab(kTabStripElementId, 0),
      OpenContextualTasksInSidePanel(kSidePanelId),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelId, 0),
      ForceClickAddContextEntrypoint(kSidePanelId),
      ForceClickMenuButton(kSidePanelId, 0),
      WaitForFaviconGroupWithTitle(kSidePanelId, "title1.html"),
      WaitForComposeboxFilesCount(kSidePanelId, 1),
      WaitForSubmitButtonEnabled(kSidePanelId),
      ClickButton(kSidePanelId, kSubmitButton),
      WaitForJsResult(kInnerWebContentsId,
                      "() => Boolean(Array.isArray(window.receivedMessages) && "
                      "window.receivedMessages.filter(buf => new "
                      "Uint8Array(buf)[0] === 18).length > 0)"),
      WithElement(kInnerWebContentsId, [](ui::TrackedElement* el) {
        auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
        std::string base64_msg =
            content::EvalJs(
                web_contents,
                "btoa(Array.from(new "
                "Uint8Array(window.receivedMessages.filter(buf => new "
                "Uint8Array(buf)[0] === 18)[0])).map(b => "
                "String.fromCharCode(b)).join(''))")
                .ExtractString();
        std::string decoded_msg;
        ASSERT_TRUE(base::Base64Decode(base64_msg, &decoded_msg));
        lens::ClientToAimMessage message;
        ASSERT_TRUE(message.ParseFromString(decoded_msg));

        EXPECT_TRUE(message.has_submit_query());
        const auto& payload = message.submit_query().payload();
        ASSERT_EQ(payload.lens_image_query_data_size(), 2);

        const auto& page_data = payload.lens_image_query_data(0);
        const auto& viewport_data = payload.lens_image_query_data(1);

        EXPECT_EQ(page_data.request_id().media_type(),
                  lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE);
        EXPECT_EQ(page_data.contextual_input_upload_type(),
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);
        EXPECT_EQ(page_data.request_id().sequence_id(), 1);
        EXPECT_EQ(page_data.request_id().image_sequence_id(), 0);
        EXPECT_EQ(page_data.request_id().long_context_id(), 1);

        EXPECT_EQ(viewport_data.request_id().media_type(),
                  lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
        EXPECT_EQ(viewport_data.contextual_input_upload_type(),
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);
        EXPECT_EQ(viewport_data.request_id().sequence_id(), 1);
        EXPECT_EQ(viewport_data.request_id().image_sequence_id(), 1);
        EXPECT_EQ(viewport_data.request_id().long_context_id(), 0);

        EXPECT_NE(page_data.request_id().uuid(),
                  viewport_data.request_id().uuid());
        EXPECT_NE(page_data.request_id().context_id(), 0);
        EXPECT_EQ(page_data.request_id().context_id(),
                  viewport_data.request_id().context_id());
      }));
}

IN_PROC_BROWSER_TEST_P(
    ContextualTasksSeparateViewportRequestIdsInteractiveUiTest,
    RecontextualizationViewportAndPageChanges) {
  SkipIfIncognito(
      "Auto-suggested tab sharing is not supported in Incognito mode.");

  const GURL kGenericPageUrl = embedded_test_server()->GetURL("/title1.html");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  uint64_t page_uuid = 0;
  uint64_t viewport_uuid = 0;
  int64_t session_context_id = 0;

  auto extract_submit_query_message =
      [](ui::TrackedElement* el,
         int message_index) -> lens::ClientToAimMessage {
    auto* web_contents = AsInstrumentedWebContents(el)->web_contents();
    std::string base64_msg =
        content::EvalJs(
            web_contents,
            base::StringPrintf(
                "btoa(Array.from(new "
                "Uint8Array(window.receivedMessages.filter(buf => new "
                "Uint8Array(buf)[0] === 18)[%d])).map(b => "
                "String.fromCharCode(b)).join(''))",
                message_index))
            .ExtractString();
    std::string decoded_msg;
    CHECK(base::Base64Decode(base64_msg, &decoded_msg));
    lens::ClientToAimMessage message;
    CHECK(message.ParseFromString(decoded_msg));
    return message;
  };

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        TestTabContextualizationController::screenshot_color_ = SK_ColorRED;
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),
      WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer),
      NavigateWebContents(kPrimaryTab, kGenericPageUrl),

      // Turn 1: Auto-suggested tab chip is submitted (initial page + viewport
      // upload).
      WaitForFaviconGroupWithTitle(kSidePanelWebContentsId, "title1.html"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),
      WaitForJsResult(kInnerWebContentsId,
                      "() => Boolean(Array.isArray(window.receivedMessages) && "
                      "window.receivedMessages.filter(buf => new "
                      "Uint8Array(buf)[0] === 18).length > 0)"),
      WithElement(
          kInnerWebContentsId,
          [&](ui::TrackedElement* el) {
            lens::ClientToAimMessage message =
                extract_submit_query_message(el, /*message_index=*/0);
            ASSERT_TRUE(message.has_submit_query());
            const auto& payload = message.submit_query().payload();
            ASSERT_EQ(payload.lens_image_query_data_size(), 2);

            const auto& page_id = payload.lens_image_query_data(0).request_id();
            const auto& viewport_id =
                payload.lens_image_query_data(1).request_id();
            page_uuid = page_id.uuid();
            viewport_uuid = viewport_id.uuid();
            session_context_id = page_id.context_id();

            EXPECT_NE(page_uuid, 0u);
            EXPECT_NE(viewport_uuid, 0u);
            EXPECT_NE(page_uuid, viewport_uuid);
            EXPECT_NE(session_context_id, 0);
            EXPECT_EQ(viewport_id.context_id(), session_context_id);

            EXPECT_EQ(page_id.media_type(),
                      lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE);
            EXPECT_EQ(page_id.sequence_id(), 1);
            EXPECT_EQ(page_id.image_sequence_id(), 0);
            EXPECT_EQ(page_id.long_context_id(), 1);

            EXPECT_EQ(viewport_id.media_type(),
                      lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
            EXPECT_EQ(viewport_id.sequence_id(), 1);
            EXPECT_EQ(viewport_id.image_sequence_id(), 1);
            EXPECT_EQ(viewport_id.long_context_id(), 0);
          }),

      // Turn 2: Submit a second query without changing either the page content
      // or the viewport screenshot; QueryContextualizer should not reupload.
      InputText(kSidePanelWebContentsId, "second query"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),
      VerifyMultipleSubmitQueryMessage("second query",
                                       /*expected_viewport_image_count=*/0,
                                       /*expected_upload_image_count=*/0,
                                       /*expected_upload_file_count=*/0,
                                       /*expected_added_input_names=*/{},
                                       /*expected_message_index=*/1),

      // Turn 3: Change only the viewport screenshot color (page content
      // unchanged).
      Do([&]() {
        TestTabContextualizationController::screenshot_color_ = SK_ColorBLUE;
      }),
      InputText(kSidePanelWebContentsId, "third query"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),
      WaitForJsResult(kInnerWebContentsId,
                      "() => Boolean(Array.isArray(window.receivedMessages) && "
                      "window.receivedMessages.filter(buf => new "
                      "Uint8Array(buf)[0] === 18).length > 2)"),
      WithElement(
          kInnerWebContentsId,
          [&](ui::TrackedElement* el) {
            lens::ClientToAimMessage message =
                extract_submit_query_message(el, /*message_index=*/2);
            ASSERT_TRUE(message.has_submit_query());
            const auto& payload = message.submit_query().payload();
            EXPECT_EQ(payload.query_text(), "third query");
            ASSERT_EQ(payload.lens_image_query_data_size(), 2);

            const auto& page_id = payload.lens_image_query_data(0).request_id();
            const auto& viewport_id =
                payload.lens_image_query_data(1).request_id();

            // Page request ID preserves uuid, context_id, and Turn 1's sequence
            // numbers because page content was not reuploaded on Turn 3.
            EXPECT_EQ(page_id.uuid(), page_uuid);
            EXPECT_EQ(page_id.context_id(), session_context_id);
            EXPECT_EQ(page_id.media_type(),
                      lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE);
            EXPECT_EQ(page_id.sequence_id(), 1);
            EXPECT_EQ(page_id.image_sequence_id(), 0);
            EXPECT_EQ(page_id.long_context_id(), 1);

            // Viewport request ID preserves uuid and context_id while
            // incrementing sequence_id and image_sequence_id to 2.
            EXPECT_EQ(viewport_id.uuid(), viewport_uuid);
            EXPECT_EQ(viewport_id.context_id(), session_context_id);
            EXPECT_EQ(viewport_id.media_type(),
                      lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
            EXPECT_EQ(viewport_id.sequence_id(), 2);
            EXPECT_EQ(viewport_id.image_sequence_id(), 2);
            EXPECT_EQ(viewport_id.long_context_id(), 0);
          }),

      // Turn 4: Change both the page DOM content and the viewport screenshot.
      WithElement(kPrimaryTab,
                  [](ui::TrackedElement* el) {
                    auto* wc = AsInstrumentedWebContents(el)->web_contents();
                    EXPECT_TRUE(content::ExecJs(
                        wc,
                        "document.body.innerText += ' Updated page content "
                        "with enough characters to exceed byte tolerance.';"));
                  }),
      Do([&]() {
        TestTabContextualizationController::screenshot_color_ = SK_ColorGREEN;
      }),
      InputText(kSidePanelWebContentsId, "fourth query"),
      ClickButton(kSidePanelWebContentsId, kSubmitButton),
      WaitForJsResult(kInnerWebContentsId,
                      "() => Boolean(Array.isArray(window.receivedMessages) && "
                      "window.receivedMessages.filter(buf => new "
                      "Uint8Array(buf)[0] === 18).length > 3)"),
      WithElement(kInnerWebContentsId, [&](ui::TrackedElement* el) {
        lens::ClientToAimMessage message =
            extract_submit_query_message(el, /*message_index=*/3);
        ASSERT_TRUE(message.has_submit_query());
        const auto& payload = message.submit_query().payload();
        EXPECT_EQ(payload.query_text(), "fourth query");
        ASSERT_EQ(payload.lens_image_query_data_size(), 2);

        const auto& page_id = payload.lens_image_query_data(0).request_id();
        const auto& viewport_id = payload.lens_image_query_data(1).request_id();

        // Page request ID increments sequence_id and long_context_id to 2.
        EXPECT_EQ(page_id.uuid(), page_uuid);
        EXPECT_EQ(page_id.context_id(), session_context_id);
        EXPECT_EQ(page_id.media_type(),
                  lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE);
        EXPECT_EQ(page_id.sequence_id(), 2);
        EXPECT_EQ(page_id.image_sequence_id(), 0);
        EXPECT_EQ(page_id.long_context_id(), 2);

        // Viewport request ID increments sequence_id and image_sequence_id
        // to 3.
        EXPECT_EQ(viewport_id.uuid(), viewport_uuid);
        EXPECT_EQ(viewport_id.context_id(), session_context_id);
        EXPECT_EQ(viewport_id.media_type(),
                  lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
        EXPECT_EQ(viewport_id.sequence_id(), 3);
        EXPECT_EQ(viewport_id.image_sequence_id(), 3);
        EXPECT_EQ(viewport_id.long_context_id(), 0);
      }));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksSeparateViewportRequestIdsInteractiveUiTest,
    testing::Values(UserVariation::kSignedIn,
                    UserVariation::kSignedOut,
                    UserVariation::kIncognito),
    &UserVariationToString);

// Enables the ephemeral branded toolbar entry point, which surfaces the
// ephemeral Contextual Tasks toolbar button after the side panel is closed on a
// tab associated with a task.
class ContextualTasksEphemeralToolbarInteractiveUiTest
    : public ContextualTasksInteractiveUiTest {
 public:
  void SetUpFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        GetDefaultEnabledFeatures();
    // The ephemeral button only shows when it matches the side panel dock.
    // Enable the right-hand variant so the test does not depend on the
    // default side panel alignment.
    enabled.push_back(
        {kContextualTasksEphemeralBrandedEntryPoint,
         {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"},
          {"enable-right-hand-contextual-tasks-ephemeral-button", "true"}}});
    enabled.push_back({kContextualTasksForceEntryPointEligibility, {}});
    feature_list_.InitWithFeaturesAndParameters(enabled,
                                                GetDefaultDisabledFeatures());
  }

  std::optional<base::Uuid> GetTaskIdForActiveTab() {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::optional<ContextualTask> task =
        CHECK_DEREF(ContextualTasksServiceFactory::GetForProfile(
                        browser()->GetProfile()))
            .GetContextualTaskForTab(
                sessions::SessionTabHelper::IdForTab(web_contents));
    if (!task.has_value()) {
      return std::nullopt;
    }
    return task->GetTaskId();
  }

  content::WebContents* GetActiveSidePanelWebContents() {
    return ContextualTasksPanelController::From(browser())
        ->GetActiveWebContents();
  }
};

// This tests the following CUJ:
//  (1) User navigates to a webpage and starts a Lens region search from the
//      Chrome app menu.
//  (2) Contextual Tasks side panel opens with an active task for the tab.
//  (3) User clicks the close button in the Contextual Tasks side panel
//      toolbar.
//  (4) Side panel closes and the ephemeral Contextual Tasks button appears in
//      the browser toolbar.
//  (5) User clicks the ephemeral toolbar button.
//  (6) Contextual Tasks side panel reopens and restores the same task and
//      side panel contents.
//  (7) Ephemeral toolbar button hides while the side panel is open.
IN_PROC_BROWSER_TEST_P(ContextualTasksEphemeralToolbarInteractiveUiTest,
                       EphemeralToolbarButtonReopen) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReopenedSidePanelId);

  const DeepQuery kCloseButton = {"contextual-tasks-app", "#toolbar",
                                  "#closeButton"};

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->GetBoundsInScreen().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  std::optional<base::Uuid> task_id_before_close;
  base::WeakPtr<content::WebContents> side_panel_contents_before_close;

  RunTestSequence(
      // (1) Navigate to a page and start a Lens region search.
      OpenLensOverlayWithRegionSearch(kPrimaryTab, kOverlayId,
                                      std::move(off_center_point)),

      // (2) The Contextual Tasks side panel opens with an active task.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "EphemeralReopenSidePanelContents",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelId, "EphemeralReopenSidePanelContents"),
      WaitForElementVisible(kSidePanelId, kCloseButton),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      Do([&]() {
        task_id_before_close = GetTaskIdForActiveTab();
        content::WebContents* const contents = GetActiveSidePanelWebContents();
        if (contents) {
          side_panel_contents_before_close = contents->GetWeakPtr();
        }
      }),
      Check([&]() { return task_id_before_close.has_value(); },
            "Active tab is associated with a task before closing"),
      Check([&]() { return !!side_panel_contents_before_close; },
            "Side panel WebContents exist before closing"),

      // (3) Close the side panel via the toolbar close button. The clicked
      // WebUI hides in response, so do not wait for a JS round trip.
      ClickElement(kSidePanelId, kCloseButton, ExecuteJsMode::kFireAndForget),

      // (4) The side panel closes. In Incognito, the ephemeral button does not
      // appear and the active tab is disassociated from the task.
      WaitForHide(kContextualTasksSidePanelWebViewElementId),
      UninstrumentWebContents(kSidePanelId,
                              /*fail_if_not_instrumented=*/false),
      GetParam() == UserVariation::kIncognito
          ? Steps(EnsureNotPresent(
                      kContextualTasksEphemeralToolbarButtonElementId),
                  Check([&]() { return !GetTaskIdForActiveTab().has_value(); },
                        "Active tab is disassociated from the task after "
                        "closing in Incognito"))
          : Steps(
                WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
                Check(
                    [&]() {
                      return GetTaskIdForActiveTab() == task_id_before_close;
                    },
                    "Active tab stays associated with the task after closing"),

                // (5) Click the ephemeral toolbar button.
                PressButton(kContextualTasksEphemeralToolbarButtonElementId),

                // (6) The side panel reopens and restores the same task and
                // contents.
                WaitForShow(kContextualTasksSidePanelWebViewElementId),
                Check(
                    [&]() {
                      return ContextualTasksPanelController::From(browser())
                          ->IsPanelOpenForContextualTask();
                    },
                    "Side panel is open for the contextual task"),
                Check(
                    [&]() {
                      return GetTaskIdForActiveTab() == task_id_before_close;
                    },
                    "Reopened side panel restores the same task"),
                Check(
                    [&]() {
                      return side_panel_contents_before_close &&
                             side_panel_contents_before_close.get() ==
                                 GetActiveSidePanelWebContents();
                    },
                    "Reopened side panel restores the same WebContents"),
                NameViewRelative(
                    kContextualTasksSidePanelWebViewElementId,
                    "EphemeralReopenedSidePanelContents",
                    [](ContextualTasksWebView* web_view) -> views::View* {
                      return web_view->content_web_view();
                    }),
                InstrumentNonTabWebView(kReopenedSidePanelId,
                                        "EphemeralReopenedSidePanelContents"),
                WaitForElementVisible(kReopenedSidePanelId, kCloseButton),

                // (7) The ephemeral button hides while the side panel is open.
                WaitForHide(kContextualTasksEphemeralToolbarButtonElementId)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksCopyUrlTest,
    testing::Combine(testing::Values(UserVariation::kSignedIn,
                                     UserVariation::kSignedOut,
                                     UserVariation::kIncognito),
                     testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<UserVariation, bool>>& info) {
      return base::StrCat(
          {UserVariationToString(testing::TestParamInfo<UserVariation>(
               std::get<0>(info.param), info.index)),
           std::get<1>(info.param) ? "_WebUI" : "_Views"});
    });

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksPinnedToolbarInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksInteractiveUiTestWithChips,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

class ContextualTasksExtensionLensButtonInteractiveUiTest
    : public ContextualTasksInteractiveUiTest {
 public:
  ContextualTasksExtensionLensButtonInteractiveUiTest() {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }
  ~ContextualTasksExtensionLensButtonInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    enabled.push_back({kContextualTasksRearchitecture, {}});
    enabled.push_back({kContextualTasksSidePanelRearchitecture, {}});
    enabled.push_back({extensions_features::kApiContextualTasksPrivate, {}});
    feature_list_.InitWithFeaturesAndParameters(enabled,
                                                GetDefaultDisabledFeatures());
  }

  void SetUpOnMainThread() override {
    ContextualTasksInteractiveUiTest::SetUpOnMainThread();
    ContextualTasksExtensionBridge::Get(browser()->GetProfile());
  }

  static content::RenderFrameHost* MountAndWaitForLensButtonFrame(
      content::WebContents* host_contents) {
    EXPECT_TRUE(content::ExecJs(host_contents, R"(
      (() => {
        let iframe = document.getElementById('lens-button-iframe');
        if (!iframe) {
          iframe = document.createElement('iframe');
          iframe.id = 'lens-button-iframe';
          document.body.appendChild(iframe);
        }
      })();
    )"));
    const GURL lens_button_url(
        base::StringPrintf("chrome-extension://%s/lens_button.html?cs=0",
                           extension_misc::kContextualTasksExtensionId));
    EXPECT_TRUE(content::NavigateIframeToURL(
        host_contents, "lens-button-iframe", lens_button_url));

    content::RenderFrameHost* button_rfh = nullptr;
    EXPECT_TRUE(base::test::RunUntil([&]() -> bool {
      button_rfh = content::FrameMatchingPredicateOrNullptr(
          host_contents->GetPrimaryPage(),
          base::BindRepeating([](content::RenderFrameHost* rfh) {
            return rfh->GetLastCommittedURL().ExtractFileName() ==
                   "lens_button.html";
          }));
      return button_rfh != nullptr;
    }));
    if (!button_rfh) {
      return nullptr;
    }

    EXPECT_TRUE(base::test::RunUntil([&]() -> bool {
      auto result = content::EvalJs(button_rfh, R"(
        (() => {
          const app = document.querySelector('lens-button-app');
          if (!app || !app.shadowRoot) return false;
          const btn = app.shadowRoot.querySelector('#lensButton');
          return Boolean(btn && !btn.disabled);
        })();
      )");
      return result.is_ok() && result.ExtractBool();
    }));
    return button_rfh;
  }
};

class ContextualTasksSidePanelRearchToolbarInteractiveUiTest
    : public ContextualTasksInteractiveUiTest {
 public:
  void SetUpFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled;
    for (const auto& f : GetDefaultEnabledFeatures()) {
      if (&f.feature.get() != &kContextualTasksHideMenuOnAiPage) {
        enabled.push_back(f);
      }
    }
    enabled.push_back({kContextualTasksRearchitecture, {}});
    enabled.push_back({kContextualTasksSidePanelRearchitecture, {}});
    enabled.push_back({kContextualTasksEnableSpatialModelToolbarLayout, {}});

    auto disabled = GetDefaultDisabledFeatures();
    std::erase(disabled, kContextualTasksRearchitecture);
    std::erase(disabled, kContextualTasksSidePanelRearchitecture);
    std::erase(disabled, kContextualTasksEnableSpatialModelToolbarLayout);
    disabled.push_back(kContextualTasksHideMenuOnAiPage);

    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

 protected:
  // Opens the Lens Overlay from the 3-dot App Menu and submits `query` in its
  // Contextual Searchbox. The search URL is mocked to `aim_url`, so the
  // Contextual Tasks side panel opens on that AI Mode page.
  MultiStep SubmitContextualSearchboxQueryToOpenSidePanel(
      ui::ElementIdentifier overlay_id,
      const std::string& query,
      const GURL& aim_url) {
    const DeepQuery kPathToOverlaySearchboxInput{
        "lens-overlay-app",
        "cr-lens-searchbox",
        "cr-searchbox-input",
        "input",
    };
    return Steps(
        OpenLensOverlay(),
        InAnyContext(
            InstrumentNonTabWebView(overlay_id,
                                    LensOverlayController::kOverlayId),
            WaitForWebContentsReady(
                overlay_id, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
        InSameContext(
            WaitForShow(LensOverlayController::kOverlayId),
            Do([this, aim_url]() {
              content::WebContents* web_contents =
                  browser()->tab_strip_model()->GetActiveWebContents();
              auto* lens_controller =
                  LensSearchController::FromTabWebContents(web_contents);
              auto* query_router = static_cast<lens::FakeLensQueryFlowRouter*>(
                  lens_controller->query_router());
              CHECK(query_router);
              auto* session_handle = static_cast<
                  contextual_search::MockContextualSearchSessionHandle*>(
                  query_router->GetContextualSearchSessionHandle());
              CHECK(session_handle);
              ON_CALL(*session_handle,
                      CreateSearchUrl(::testing::_, ::testing::_))
                  .WillByDefault(::testing::WithArg<1>(
                      [aim_url](base::OnceCallback<void(GURL)> callback) {
                        std::move(callback).Run(aim_url);
                      }));
            }),
            WaitForElementExists(overlay_id, kPathToOverlaySearchboxInput),
            FocusWebContents(overlay_id),
            ExecuteJsAt(overlay_id, kPathToOverlaySearchboxInput,
                        "(el) => { el.focus(); }",
                        ExecuteJsMode::kWaitForCompletion),
            ExecuteJsAt(
                overlay_id, kPathToOverlaySearchboxInput,
                content::JsReplace(
                    "(el) => { el.value = $1; el.dispatchEvent(new "
                    "Event('input', { bubbles: true })); el.dispatchEvent(new "
                    "Event('change', { bubbles: true })); }",
                    query),
                ExecuteJsMode::kWaitForCompletion),
            ExecuteJsAt(overlay_id, kPathToOverlaySearchboxInput,
                        "(el) => { el.dispatchEvent(new KeyboardEvent("
                        "'keydown', { key: 'Enter', bubbles: true, "
                        "cancelable: true, composed: true })); }",
                        ExecuteJsMode::kFireAndForget)),
        WaitForShow(kContextualTasksSidePanelWebViewElementId));
  }

  // Instruments the side panel's rearchitecture toolbar WebView (which loads
  // chrome://contextual-tasks/toolbar.html) as `toolbar_id`.
  MultiStep InstrumentSidePanelToolbar(ui::ElementIdentifier toolbar_id) {
    return Steps(
        NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                         "SidePanelToolbarWebViewName",
                         [](ContextualTasksWebView* web_view) -> views::View* {
                           return web_view->toolbar_web_view();
                         }),
        InstrumentNonTabWebView(toolbar_id, "SidePanelToolbarWebViewName"));
  }
};

// This tests the following CUJ:
// 1. User navigates a regular browser tab to a Google page that embeds the
//    Contextual Tasks extension's lens_button.html iframe.
// 2. User clicks the embedded Lens button in the regular tab; because the
//    button is not embedded in the Contextual Tasks side panel, nothing happens
//    and the Lens overlay does not open.
// 3. User opens the Contextual Tasks side panel, which embeds lens_button.html,
//    and clicks the Lens button; the Lens overlay opens.
IN_PROC_BROWSER_TEST_P(ContextualTasksExtensionLensButtonInteractiveUiTest,
                       LensButtonClickOnlyWorksInSidePanel) {
  SkipIfIncognito(
      "Contextual Tasks component extension is not enabled in Incognito.");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  const GURL kRegularTabUrl("https://www.google.com/search?q=regular_tab");
  const GURL kAimUrl("https://www.google.com/search?udm=50");
  base::UserActionTester user_action_tester;

  RunTestSequence(
      // 1. Navigate the regular tab to a Google page and embed
      // lens_button.html.
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      NavigateWebContents(kPrimaryTab, kRegularTabUrl),
      WaitForWebContentsPainted(kPrimaryTab),
      WaitForWebContentsReady(kPrimaryTab, kRegularTabUrl),

      // 2. Click the Lens button inside the regular tab's iframe and verify it
      // is a no-op.
      Do([&]() {
        content::WebContents* tab_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        ASSERT_NE(tab_contents, nullptr);
        content::RenderFrameHost* tab_button_rfh =
            MountAndWaitForLensButtonFrame(tab_contents);
        ASSERT_NE(tab_button_rfh, nullptr);

        // Click #lensButton and perform a round-trip call on the same
        // composebox::mojom::PageHandler remote pipe so HandleLensButtonClick()
        // is guaranteed to have been processed on the browser UI thread.
        EXPECT_EQ(true, content::EvalJs(tab_button_rfh, R"(
          (async () => {
            const app = document.querySelector('lens-button-app');
            const btn = app.shadowRoot.querySelector('#lensButton');
            btn.click();
            await app.getPageHandler_().getSmartTabSharingActive();
            return !app.active;
          })();
        )"));

        auto* search_controller =
            LensSearchController::FromTabWebContents(tab_contents);
        ASSERT_NE(search_controller, nullptr);
        EXPECT_TRUE(search_controller->IsOff());
        EXPECT_FALSE(search_controller->IsShowingUI());
        EXPECT_EQ(
            0, user_action_tester.GetActionCount(
                   "ContextualTasks.Composebox.UserAction.LensButtonClicked"));
      }),
      EnsureNotPresent(LensOverlayController::kOverlayId),

      // 3. Open the Contextual Tasks side panel and embed lens_button.html
      // inside the side panel WebContents.
      Do([&]() {
        ContextualTasksUiService* ui_service =
            ContextualTasksUiServiceFactory::GetForBrowserContext(
                browser()->GetProfile());
        tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
        ui_service->StartTaskUiInSidePanel(browser(), tab, kAimUrl, nullptr);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      WaitForWebContentsReady(kSidePanelWebContentsId),

      // 4. Click the Lens button inside the side panel's iframe and verify the
      // Lens overlay opens.
      Do([&]() {
        content::WebContents* side_panel_contents =
            ContextualTasksPanelController::From(browser())
                ->GetActiveWebContents();
        ASSERT_NE(side_panel_contents, nullptr);
        content::RenderFrameHost* panel_button_rfh =
            MountAndWaitForLensButtonFrame(side_panel_contents);
        ASSERT_NE(panel_button_rfh, nullptr);

        EXPECT_EQ(true, content::EvalJs(panel_button_rfh, R"(
          (() => {
            const app = document.querySelector('lens-button-app');
            const btn = app.shadowRoot.querySelector('#lensButton');
            btn.click();
            return true;
          })();
        )"));
      }),
      WaitForShow(LensOverlayController::kOverlayId), Do([&]() {
        content::WebContents* tab_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        auto* search_controller =
            LensSearchController::FromTabWebContents(tab_contents);
        ASSERT_NE(search_controller, nullptr);
        EXPECT_TRUE(search_controller->IsShowingUI());
        EXPECT_EQ(
            1, user_action_tester.GetActionCount(
                   "ContextualTasks.Composebox.UserAction.LensButtonClicked"));
      }));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksExtensionLensButtonInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

class ContextualTasksExtensionTabPickerInteractiveUiTest
    : public ContextualTasksInteractiveUiTest {
 public:
  ContextualTasksExtensionTabPickerInteractiveUiTest() {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }
  ~ContextualTasksExtensionTabPickerInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    enabled.push_back({kContextualTasksRearchitecture, {}});
    enabled.push_back({kContextualTasksSidePanelRearchitecture, {}});
    enabled.push_back({kContextualTasksTabBasedSuggestions, {}});
    enabled.push_back({kEnableContextualTasksPinButtonInToolbar, {}});
    enabled.push_back({kContextualTasksForceEntryPointEligibility, {}});
    enabled.push_back({extensions_features::kApiContextualTasksPrivate, {}});
    feature_list_.InitWithFeaturesAndParameters(enabled,
                                                GetDefaultDisabledFeatures());
  }

  void SetUpOnMainThread() override {
    ContextualTasksInteractiveUiTest::SetUpOnMainThread();
    ContextualTasksExtensionBridge::Get(browser()->GetProfile());
    PinnedToolbarActionsModel::Get(browser()->GetProfile())
        ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  }

  static content::RenderFrameHost* MountAndWaitForTabPickerFrame(
      content::WebContents* host_contents) {
    EXPECT_EQ(true, content::EvalJs(host_contents, R"(
      new Promise((resolve) => {
        const check = () => {
          if (Array.isArray(window.receivedMessages)) {
            resolve(true);
            return;
          }
          setTimeout(check, 10);
        };
        check();
      })
    )"));

    const GURL tab_picker_url(
        base::StringPrintf("chrome-extension://%s/tab_picker.html?cs=0",
                           extension_misc::kContextualTasksExtensionId));
    EXPECT_TRUE(content::ExecJs(
        host_contents,
        content::JsReplace(
            R"(
      (() => {
        if (!Array.isArray(window.receivedSearchMessages)) {
          window.receivedSearchMessages = [];
          window.searchMessageWaiters = [];
          const port = chrome.runtime.connect(
              $1, {name: 'chrome-search-channel'});
          window.__aimSearchPort = port;
          window.__handshakeCompletePromise = new Promise((resolve) => {
            port.onMessage.addListener((msg) => {
              if (!msg || msg.type !== 'chrome-search-channel' ||
                  !Array.isArray(msg.payload)) {
                return;
              }
              if (msg.payload.length >= 2 && msg.payload[0] === 10) {
                port.postMessage({
                  type: 'chrome-search-channel',
                  payload: [10, 0],
                });
                resolve(true);
                return;
              }
              window.receivedSearchMessages.push(
                  btoa(Array.from(msg.payload, b => String.fromCharCode(b))
                      .join('')));
              const waiters = window.searchMessageWaiters.splice(0);
              for (const w of waiters) {
                w();
              }
            });
          });
        }
        let iframe = document.getElementById('tab-picker-iframe');
        if (!iframe) {
          iframe = document.createElement('iframe');
          iframe.id = 'tab-picker-iframe';
          document.body.appendChild(iframe);
        }
      })();
    )",
            extension_misc::kContextualTasksExtensionId)));
    EXPECT_TRUE(content::NavigateIframeToURL(host_contents, "tab-picker-iframe",
                                             tab_picker_url));

    content::RenderFrameHost* picker_rfh = nullptr;
    EXPECT_TRUE(base::test::RunUntil([&]() -> bool {
      picker_rfh = content::FrameMatchingPredicateOrNullptr(
          host_contents->GetPrimaryPage(),
          base::BindRepeating([](content::RenderFrameHost* rfh) {
            return rfh->GetLastCommittedURL().ExtractFileName() ==
                   "tab_picker.html";
          }));
      return picker_rfh != nullptr;
    }));
    if (!picker_rfh) {
      return nullptr;
    }

    EXPECT_EQ(true, content::EvalJs(picker_rfh, R"(
      new Promise((resolve) => {
        const check = () => {
          const app = document.querySelector('tab-picker-app');
          const trigger = app?.shadowRoot?.querySelector('#shareTabsTrigger');
          const item = app?.shadowRoot?.querySelector(
              'button.dropdown-item[data-index="0"]');
          if (trigger && item && document.readyState === 'complete') {
            resolve(true);
            return;
          }
          setTimeout(check, 10);
        };
        check();
      })
    )"));

    EXPECT_EQ(true, content::EvalJs(host_contents,
                                    "window.__handshakeCompletePromise"));
    auto* user_data =
        ContextualTasksWebContentsUserData::FromWebContents(host_contents);
    EXPECT_NE(user_data, nullptr);
    if (user_data) {
      EXPECT_TRUE(base::test::RunUntil(
          [&]() { return user_data->IsHandshakeCompleteForTesting(); }));
    }
    return picker_rfh;
  }

  static bool WaitForOnSuggestInputsChanged(
      content::WebContents* host_contents,
      int expected_contexts_count,
      lens::ClientToSearchMessage* out_message = nullptr) {
    for (int attempt = 0; attempt < 20; ++attempt) {
      auto result = content::EvalJs(host_contents, content::JsReplace(
                                                       R"(
        new Promise((resolve) => {
          const minLen = $1;
          const check = () => {
            if (Array.isArray(window.receivedSearchMessages) &&
                window.receivedSearchMessages.length >= minLen) {
              resolve(window.receivedSearchMessages.slice());
              return;
            }
            window.searchMessageWaiters.push(check);
          };
          check();
        })
      )",
                                                       attempt + 1));
      if (!result.is_ok()) {
        return false;
      }
      const auto& list = result.ExtractList();
      for (auto it = list.rbegin(); it != list.rend(); ++it) {
        if (!it->is_string()) {
          continue;
        }
        std::string decoded;
        if (!base::Base64Decode(it->GetString(), &decoded)) {
          continue;
        }
        lens::ClientToSearchMessage message;
        if (!message.ParseFromString(decoded) ||
            !message.has_on_suggest_inputs_changed()) {
          continue;
        }
        if (message.on_suggest_inputs_changed().added_contexts_size() ==
            expected_contexts_count) {
          if (out_message) {
            *out_message = std::move(message);
          }
          return true;
        }
        break;
      }
    }
    return false;
  }
};

// This tests the following CUJ:
//  (1) User navigates a browser tab to an eligible webpage.
//  (2) User clicks the pinned Contextual Tasks toolbar button to open the
//      Contextual Tasks side panel on an AI Mode page.
//  (3) The AI Mode page mounts the Contextual Tasks extension's
//      tab_picker.html iframe.
//  (4) User clicks the tab picker trigger to open the tab menu and selects
//      the open browser tab to attach its context.
//  (5) The tab item shows as selected in the tab picker and the AI Mode page
//      receives an OnSuggestInputsChanged message with the uploaded tab's
//      AddedContext.
//  (6) User clicks the selected tab item in the tab picker again to unselect
//      and remove its context.
//  (7) The tab item updates to unselected and the AI Mode page receives an
//      empty OnSuggestInputsChanged message.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksExtensionTabPickerInteractiveUiTest,
    TabPickerSelectAndUnselectTabEmitsOnSuggestInputsChanged) {
  SkipIfIncognito(
      "Contextual Tasks component extension is not enabled in Incognito.");

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  const GURL kArticleUrl = embedded_test_server()->GetURL("/title1.html");
  content::RenderFrameHost* tab_picker_rfh = nullptr;

  RunTestSequence(
      // (1) Navigate the primary tab to an eligible webpage.
      InstrumentTab(kPrimaryTab, 0), SelectTab(kTabStripElementId, 0),
      NavigateWebContents(kPrimaryTab, kArticleUrl),
      WaitForWebContentsPainted(kPrimaryTab),
      WaitForWebContentsReady(kPrimaryTab, kArticleUrl),

      // (2) Click the pinned Contextual Tasks toolbar button to open the side
      // panel on the AI Mode page.
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "TabPickerSidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "TabPickerSidePanelContentWebViewName"),
      WaitForWebContentsReady(kSidePanelWebContentsId),

      // (3)-(5) Mount tab_picker.html in the AI Mode page, open the tab menu,
      // select the tab, and verify the UI selection state and emitted
      // OnSuggestInputsChanged message.
      Do([&]() {
        content::WebContents* side_panel_contents =
            ContextualTasksPanelController::From(browser())
                ->GetActiveWebContents();
        ASSERT_NE(side_panel_contents, nullptr);
        tab_picker_rfh = MountAndWaitForTabPickerFrame(side_panel_contents);
        ASSERT_NE(tab_picker_rfh, nullptr);

        EXPECT_EQ(true, content::EvalJs(tab_picker_rfh, R"(
          (async () => {
            const app = document.querySelector('tab-picker-app');
            const trigger = app.shadowRoot.querySelector('#shareTabsTrigger');
            trigger.click();
            const item = app.shadowRoot.querySelector(
                'button.dropdown-item[data-index="0"]');
            item.click();
            await app.updateComplete;
            return app.selectedTabs.length === 1 &&
                Boolean(item.querySelector('.share-tabs-check'));
          })();
        )"));

        lens::ClientToSearchMessage add_message;
        ASSERT_TRUE(WaitForOnSuggestInputsChanged(
            side_panel_contents, /*expected_contexts_count=*/1, &add_message));
        ASSERT_EQ(add_message.on_suggest_inputs_changed().added_contexts_size(),
                  1);
        const auto& added =
            add_message.on_suggest_inputs_changed().added_contexts(0);
        EXPECT_EQ(added.search_session_id(), "test_search_session_id");
        EXPECT_GT(added.request_id().uuid(), 0u);
        EXPECT_EQ(added.request_id().media_type(),
                  lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE);
        EXPECT_EQ(added.contextual_input_upload_type(),
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);
      }),

      // (6)-(7) Click the selected tab item again to unselect it and verify
      // the tab is unselected and an empty OnSuggestInputsChanged message is
      // emitted.
      Do([&]() {
        content::WebContents* side_panel_contents =
            ContextualTasksPanelController::From(browser())
                ->GetActiveWebContents();
        ASSERT_NE(side_panel_contents, nullptr);
        ASSERT_NE(tab_picker_rfh, nullptr);

        EXPECT_TRUE(content::ExecJs(side_panel_contents,
                                    "window.receivedSearchMessages = [];"));

        EXPECT_EQ(true, content::EvalJs(tab_picker_rfh, R"(
          (async () => {
            const app = document.querySelector('tab-picker-app');
            const item = app.shadowRoot.querySelector(
                'button.dropdown-item[data-index="0"]');
            item.click();
            await app.updateComplete;
            return app.selectedTabs.length === 0 &&
                !item.querySelector('.share-tabs-check');
          })();
        )"));

        lens::ClientToSearchMessage clear_message;
        ASSERT_TRUE(WaitForOnSuggestInputsChanged(side_panel_contents,
                                                  /*expected_contexts_count=*/0,
                                                  &clear_message));
        EXPECT_EQ(
            clear_message.on_suggest_inputs_changed().added_contexts_size(), 0);
      }));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksExtensionTabPickerInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

// This tests the following CUJ:
//  (1) User opens Lens Overlay from the 3-dot App Menu on a web page.
//  (2) User types a query into the Lens Overlay Contextual Searchbox and
//      presses Enter to submit.
//  (3) The Contextual Tasks side panel opens and navigates to an AI Mode page.
//  (4) The side panel toolbar updates its thread title to match the AI query.
//  (5) User clicks the toolbar overflow menu button and verifies the Thread
//      History menu item is present and visible in the menu.
IN_PROC_BROWSER_TEST_P(ContextualTasksSidePanelRearchToolbarInteractiveUiTest,
                       AiPageUpdatesThreadTitleAndShowsThreadHistoryInMenu) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelToolbarWebContentsId);

  const DeepQuery kToolbarTitle{
      "contextual-tasks-toolbar-app",
      "#toolbar",
      ".top-toolbar-title",
  };
  const DeepQuery kOverflowMenuButton{
      "contextual-tasks-toolbar-app",
      "#toolbar",
      "#overflowMenuButton",
  };
  const DeepQuery kOverflowMenu{
      "contextual-tasks-toolbar-app",
      "#toolbar",
      "contextual-tasks-overflow-menu",
  };

  RunTestSequence(
      // Steps 1-3: Open the Lens Overlay via the 3-dot App Menu and submit a
      // query in its Contextual Searchbox to open the Contextual Tasks side
      // panel on an AI page, then instrument the rearchitecture toolbar.
      SubmitContextualSearchboxQueryToOpenSidePanel(
          kOverlayId, "hello world",
          GURL("https://www.google.com/search?q=hello+world&udm=50")),
      InstrumentSidePanelToolbar(kSidePanelToolbarWebContentsId),

      // Step 4: Verify the thread title in the toolbar is updated to
      // "hello world".
      WaitForElementExists(kSidePanelToolbarWebContentsId, kToolbarTitle),
      WaitForJsResultAt(kSidePanelToolbarWebContentsId, kToolbarTitle,
                        "el => el.textContent.trim()",
                        std::string("hello world")),

      // Step 5: Open the toolbar overflow menu and verify the Thread History
      // menu item is present and visible.
      WaitForElementVisible(kSidePanelToolbarWebContentsId,
                            kOverflowMenuButton),
      ClickButton(kSidePanelToolbarWebContentsId, kOverflowMenuButton),
      WaitForElementExists(kSidePanelToolbarWebContentsId, kOverflowMenu),
      WaitForJsResultAt(
          kSidePanelToolbarWebContentsId, kOverflowMenu,
          content::JsReplace(
              "el => Boolean(el.shadowRoot.querySelector('#menu')?.open && "
              "Array.from(el.shadowRoot.querySelectorAll("
              "'button.dropdown-item')).some(b => !b.hidden && "
              "b.textContent.trim() === $1))",
              l10n_util::GetStringUTF8(
                  IDS_CONTEXTUAL_TASKS_SIDE_PANEL_HISTORY_TOOL_TIP)),
          true));
}

// This tests the following CUJ:
//  (1) User opens Lens Overlay from the 3-dot App Menu on a web page.
//  (2) User types a query into the Lens Overlay Contextual Searchbox and
//      presses Enter to submit.
//  (3) The Contextual Tasks side panel opens on an AI Mode page.
//  (4) User clicks the Super G logo button in the side panel toolbar.
//  (5) The Page Info bubble opens anchored to the Super G button inside the
//      side panel with a TOP_LEFT arrow.
//  (6) The Page Info bubble shows the URL and site of the page in the side
//      panel, not the page in the active tab.
IN_PROC_BROWSER_TEST_P(ContextualTasksSidePanelRearchToolbarInteractiveUiTest,
                       SuperGLogoButtonOpensPageInfoBubble) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelToolbarWebContentsId);

  const DeepQuery kSuperGButton{
      "contextual-tasks-toolbar-app",
      "#toolbar",
      ".top-toolbar-logo-button",
  };
  const GURL kAimUrl("https://www.google.com/search?q=hello+world&udm=50");

  gfx::Rect side_panel_bounds;
  gfx::Rect super_g_bounds;

  RunTestSequence(
      // Steps 1-3: Open the Contextual Tasks side panel on an AI page from a
      // Contextual Searchbox query, then instrument the rearchitecture toolbar.
      SubmitContextualSearchboxQueryToOpenSidePanel(kOverlayId, "hello world",
                                                    kAimUrl),
      InstrumentSidePanelToolbar(kSidePanelToolbarWebContentsId),
      EnsureNotPresent(
          PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier),

      // Precondition: the side panel shows the AI page (the browser appends
      // extra search params to `kAimUrl`) and the active tab is on a different
      // site, so the URL checks below can tell them apart.
      Check(
          [&]() {
            content::WebContents* panel_contents =
                ContextualTasksPanelController::From(browser())
                    ->GetActiveWebContents();
            content::WebContents* tab_contents =
                browser()->tab_strip_model()->GetActiveWebContents();
            return panel_contents &&
                   panel_contents->GetVisibleURL().host() == kAimUrl.host() &&
                   panel_contents->GetVisibleURL().path() == kAimUrl.path() &&
                   tab_contents->GetVisibleURL().host() != kAimUrl.host();
          },
          "The side panel shows the AI page and the tab is on another site."),

      // Step 4: Wait for the Super G button to be tracked and laid out, then
      // click it.
      InAnyContext(WaitForShow(kContextualTasksSuperGButtonElementId)),
      WaitForElementVisible(kSidePanelToolbarWebContentsId, kSuperGButton),
      WithView(kContextualTasksSidePanelWebViewElementId,
               [&side_panel_bounds](views::View* side_panel) {
                 side_panel_bounds = side_panel->GetBoundsInScreen();
               }),
      InAnyContext(WithElement(kContextualTasksSuperGButtonElementId,
                               [&super_g_bounds](ui::TrackedElement* el) {
                                 super_g_bounds = el->GetScreenBounds();
                               })),
      ClickElement(kSidePanelToolbarWebContentsId, kSuperGButton),

      // Step 5: The Page Info bubble opens for the side panel's WebContents
      // (not the active tab), anchored to the Super G button inside the side
      // panel with a TOP_LEFT arrow.
      WaitForShow(PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier),
      WaitForShow(PageInfoMainView::kMainLayoutElementId),
      Check(
          [&]() {
            content::WebContents* panel_contents =
                ContextualTasksPanelController::From(browser())
                    ->GetActiveWebContents();
            return panel_contents &&
                   PageInfoBubbleViewBase::GetPageInfoBubbleForTesting(
                       panel_contents) != nullptr &&
                   !PageInfoBubbleViewBase::IsShowing(
                       browser()->tab_strip_model()->GetActiveWebContents());
          },
          "Page Info is shown for the side panel WebContents, not the tab."),
      // Step 6: Page Info's site URL is the side panel's URL, not the active
      // tab's.
      CheckView(
          PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier,
          [&](PageInfoBubbleView* bubble) {
            const GURL& site_url = bubble->presenter_for_testing()->site_url();
            content::WebContents* panel_contents =
                ContextualTasksPanelController::From(browser())
                    ->GetActiveWebContents();
            content::WebContents* tab_contents =
                browser()->tab_strip_model()->GetActiveWebContents();
            return panel_contents &&
                   site_url == panel_contents->GetVisibleURL() &&
                   site_url != tab_contents->GetVisibleURL();
          },
          true),
      // The bubble header shows the registrable domain of the side panel's
      // page; the active tab's page is on the embedded test server.
      CheckView(
          PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier,
          [](PageInfoBubbleView* bubble) {
            return bubble->presenter_for_testing()->GetSubjectNameForDisplay();
          },
          u"google.com"),
      CheckView(
          PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier,
          [](views::BubbleDialogDelegateView* bubble) {
            return bubble->arrow();
          },
          views::BubbleBorder::TOP_LEFT),
      CheckView(
          PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier,
          [&](views::BubbleDialogDelegateView* bubble) {
            const gfx::Rect anchor_rect = bubble->GetAnchorRect();
            return !super_g_bounds.IsEmpty() &&
                   anchor_rect.Intersects(super_g_bounds) &&
                   side_panel_bounds.Contains(anchor_rect);
          },
          true));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksSidePanelRearchToolbarInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksEphemeralToolbarInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

IN_PROC_BROWSER_TEST_F(ContextualTasksInteractiveUiTestBase,
                       SignedOut_DoesNotShowOAuthErrorDialog) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  const DeepQuery kErrorDialog = {"contextual-tasks-app",
                                  "contextual-tasks-error-dialog"};
  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  RunTestSequence(
      Do([this]() {
        GetTestContextualTasksUiService(browser()->GetProfile())
            ->SetSignedIn(false);
      }),
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),
      CheckJsResult(
          kInnerWebContentsId,
          "() => fetch('https://www.google.com/complete/search?q=test').then(r "
          "=> r.ok)",
          true),
      Do([this]() {
        EXPECT_EQ(GetTestContextualTasksUiService(browser()->GetProfile())
                      ->access_token_request_count(),
                  0);
        EXPECT_TRUE(last_auth_header().empty());
      }),
      WaitForElementDoesNotExist(kSidePanelWebContentsId, kErrorDialog),
      UninstrumentWebContents(kInnerWebContentsId,
                              /*fail_if_not_instrumented=*/false));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksInteractiveUiTestBase,
    SignInMidSession_ShowsOAuthErrorDialogAndReloadsOnClick) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  const DeepQuery kErrorDialog = {"contextual-tasks-app",
                                  "contextual-tasks-error-dialog"};
  const DeepQuery kReloadButton = {"contextual-tasks-app",
                                   "contextual-tasks-error-dialog",
                                   "cr-button.action-button"};
  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  RunTestSequence(
      Do([this]() {
        GetTestContextualTasksUiService(browser()->GetProfile())
            ->SetSignedIn(false);
      }),
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),
      Do([this]() {
        auto* service =
            GetTestContextualTasksUiService(browser()->GetProfile());
        EXPECT_EQ(service->access_token_request_count(), 0);
        EXPECT_TRUE(last_auth_header().empty());
        service->SetSignedIn(true);
      }),
      CheckJsResult(
          kInnerWebContentsId,
          "() => fetch('https://www.google.com/complete/search?q=test').then(r "
          "=> r.ok)",
          true),
      Do([this]() {
        EXPECT_EQ(GetTestContextualTasksUiService(browser()->GetProfile())
                      ->access_token_request_count(),
                  0);
        EXPECT_TRUE(last_auth_header().empty());
      }),
      WaitForElementExists(kSidePanelWebContentsId, kErrorDialog),
      WaitForElementVisible(kSidePanelWebContentsId, kReloadButton),
      UninstrumentWebContents(kInnerWebContentsId,
                              /*fail_if_not_instrumented=*/false),
      ClickElement(kSidePanelWebContentsId, kReloadButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForWebContentsNavigation(kSidePanelWebContentsId),
      WaitForElementDoesNotExist(kSidePanelWebContentsId, kErrorDialog),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),
      CheckJsResult(
          kInnerWebContentsId,
          "() => fetch('https://www.google.com/complete/search?q=test').then(r "
          "=> r.ok)",
          true),
      Do([this]() {
        EXPECT_GT(GetTestContextualTasksUiService(browser()->GetProfile())
                      ->access_token_request_count(),
                  0);
        EXPECT_EQ(last_auth_header(), "Bearer fake_access_token");
      }),
      UninstrumentWebContents(kInnerWebContentsId,
                              /*fail_if_not_instrumented=*/false));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksInteractiveUiTestBase,
    SignOutMidSession_ShowsOAuthErrorDialogAndReloadsOnClick) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  const DeepQuery kErrorDialog = {"contextual-tasks-app",
                                  "contextual-tasks-error-dialog"};
  const DeepQuery kReloadButton = {"contextual-tasks-app",
                                   "contextual-tasks-error-dialog",
                                   "cr-button.action-button"};
  ContextualTasksPanelController* coordinator =
      ContextualTasksPanelController::From(browser());

  RunTestSequence(
      Do([this]() {
        GetTestContextualTasksUiService(browser()->GetProfile())
            ->SetSignedIn(true);
      }),
      InstrumentTab(kPrimaryTab, 0), Do([&]() {
        coordinator->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),
      Do([this]() {
        auto* service =
            GetTestContextualTasksUiService(browser()->GetProfile());
        EXPECT_GT(service->access_token_request_count(), 0);
        EXPECT_EQ(last_auth_header(), "Bearer fake_access_token");
        service->ResetAccessTokenRequestCount();
        ClearLastAuthHeader();
        service->SetSignedIn(false);
      }),
      CheckJsResult(
          kInnerWebContentsId,
          "() => fetch('https://www.google.com/complete/search?q=test').then(r "
          "=> r.ok)",
          true),
      Do([this]() {
        EXPECT_EQ(GetTestContextualTasksUiService(browser()->GetProfile())
                      ->access_token_request_count(),
                  0);
        EXPECT_TRUE(last_auth_header().empty());
      }),
      WaitForElementExists(kSidePanelWebContentsId, kErrorDialog),
      WaitForElementVisible(kSidePanelWebContentsId, kReloadButton),
      UninstrumentWebContents(kInnerWebContentsId,
                              /*fail_if_not_instrumented=*/false),
      ClickElement(kSidePanelWebContentsId, kReloadButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForWebContentsNavigation(kSidePanelWebContentsId),
      WaitForElementDoesNotExist(kSidePanelWebContentsId, kErrorDialog),
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),
      CheckJsResult(
          kInnerWebContentsId,
          "() => fetch('https://www.google.com/complete/search?q=test').then(r "
          "=> r.ok)",
          true),
      Do([this]() {
        EXPECT_EQ(GetTestContextualTasksUiService(browser()->GetProfile())
                      ->access_token_request_count(),
                  0);
        EXPECT_TRUE(last_auth_header().empty());
      }),
      UninstrumentWebContents(kInnerWebContentsId,
                              /*fail_if_not_instrumented=*/false));
}

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabStateSidePanelToolbarId);

constexpr char kTabStateSidePanelToolbarName[] = "TabStateSidePanelToolbar";
constexpr char kTabStateThreadId[] = "thread1";
constexpr char kTabStateThreadUrl[] =
    "https://www.google.com/search?udm=50&q=Title+1&mtid=thread1";

std::string RearchitectureParamName(const testing::TestParamInfo<bool>& info) {
  return info.param ? "RearchitectureEnabled" : "RearchitectureDisabled";
}

}  // namespace

// Tests that closing the side panel or changing its thread only applies to the
// tab where the user took the action, and that reopening a closed tab restores
// its open panel. The test param controls whether the Contextual Tasks
// rearchitecture is enabled.
class ContextualTasksSidePanelTabStateInteractiveUiTest
    : public ContextualTasksInteractiveUiTestBase,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    enabled.push_back({kContextualTasksEphemeralBrandedEntryPoint,
                       {{"ContextualTasksEntryPoint", entry_point_}}});
    if (notify_zero_state_rendered_) {
      enabled.push_back({kEnableNotifyZeroStateRenderedCapability, {}});
    }
    auto disabled = GetDefaultDisabledFeatures();
    if (tab_state_enabled_) {
      enabled.push_back({kContextualTasksSidePanelTabState, {}});
    } else {
      disabled.push_back(kContextualTasksSidePanelTabState);
    }
    if (IsRearchitectureEnabled()) {
      enabled.push_back({kContextualTasksRearchitecture, {}});
      enabled.push_back({kContextualTasksSidePanelRearchitecture, {}});
    } else {
      disabled.push_back(kContextualTasksRearchitecture);
      disabled.push_back(kContextualTasksSidePanelRearchitecture);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

 protected:
  bool IsRearchitectureEnabled() const { return GetParam(); }

  ContextualTasksService* GetContextualTasksService() {
    return ContextualTasksServiceFactory::GetForProfile(
        browser()->GetProfile());
  }

  ContextualTasksPanelController* GetPanelController() {
    return ContextualTasksPanelController::From(browser());
  }

  SessionID GetTabId(int index) {
    return sessions::SessionTabHelper::IdForTab(
        browser()->tab_strip_model()->GetWebContentsAt(index));
  }

  std::optional<base::Uuid> GetTaskIdForTab(int index) {
    std::optional<ContextualTask> task =
        GetContextualTasksService()->GetContextualTaskForTab(GetTabId(index));
    if (!task) {
      return std::nullopt;
    }
    return task->GetTaskId();
  }

  void ActivateTab(int index) {
    browser()->tab_strip_model()->ActivateTabAt(index);
  }

  content::WebContents* GetPanelContents() {
    return GetPanelController()->GetActiveWebContents();
  }

  bool IsPanelOpen() {
    return GetPanelController()->IsPanelOpenForContextualTask();
  }

  // Returns whether `panel_contents` shows the thread with `kTabStateThreadId`.
  bool ShowsThread(content::WebContents* panel_contents) {
    if (!panel_contents) {
      return false;
    }
    if (IsRearchitectureEnabled()) {
      // The rearchitecture panel shows the AI page directly.
      std::string thread_id;
      return net::GetValueForKeyInQuery(panel_contents->GetLastCommittedURL(),
                                        "mtid", &thread_id) &&
             thread_id == kTabStateThreadId;
    }
    // The legacy panel shows a WebUI that embeds the AI page.
    content::WebUI* web_ui = panel_contents->GetWebUI();
    return web_ui && static_cast<ContextualTasksUI*>(web_ui->GetController())
                             ->GetThreadId() == kTabStateThreadId;
  }

  // Adds tab1 and tab2, and opens the panel on tab0 with a thread. tab1 and
  // tab2 aren't associated with a task.
  MultiStep OpenPanelWithThread() {
    return Steps(Do([this]() {
                   chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
                   chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
                   ContextualTasksUiServiceFactory::GetForBrowserContext(
                       browser()->GetProfile())
                       ->StartTaskUiInSidePanel(
                           browser(), browser()->GetActiveTabInterface(),
                           GURL(kTabStateThreadUrl), nullptr);
                 }),
                 WaitForShow(kContextualTasksSidePanelWebViewElementId),
                 PollUntil([this]() { return ShowsThread(GetPanelContents()); },
                           "Wait for the thread to load in the panel."),
                 Do([this]() {
                   std::optional<base::Uuid> task_id = GetTaskIdForTab(0);
                   ASSERT_TRUE(task_id.has_value());
                   shared_task_id_ = *task_id;
                 }));
  }

  // Adds tab1 and tab2, opens the panel on tab0 with a thread, and associates
  // tab2 with the same task, as if it was opened from the panel. tab1 isn't
  // associated with a task.
  MultiStep OpenPanelWithSharedThread() {
    return Steps(OpenPanelWithThread(), Do([this]() {
                   GetContextualTasksService()->AssociateTabWithTask(
                       shared_task_id_, GetTabId(2));
                 }));
  }

  // Closes tab0 so that it can be reopened from recently closed tabs, and then
  // reopens it, as Ctrl+Shift+T does.
  MultiStep CloseAndRestoreTab0() {
    return Steps(Do([this]() {
                   browser()->tab_strip_model()->CloseWebContentsAt(
                       0, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
                   ASSERT_EQ(2, browser()->tab_strip_model()->count());
                   chrome::RestoreTab(browser());
                 }),
                 PollUntil(
                     [this]() {
                       return browser()->tab_strip_model()->count() == 3 &&
                              browser()->tab_strip_model()->active_index() == 0;
                     },
                     "Wait for tab0 to be reopened."));
  }

  // Instruments the WebView that hosts the side panel toolbar as
  // `kTabStateSidePanelToolbarId`. The toolbar is part of the panel content's
  // WebUI in the legacy UI, and a separate WebUI in the rearchitecture.
  MultiStep InstrumentSidePanelToolbar() {
    if (IsRearchitectureEnabled()) {
      return Steps(NameViewRelative(
                       kContextualTasksSidePanelWebViewElementId,
                       kTabStateSidePanelToolbarName,
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->toolbar_web_view();
                       }),
                   InstrumentNonTabWebView(kTabStateSidePanelToolbarId,
                                           kTabStateSidePanelToolbarName));
    }
    return Steps(
        NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                         kTabStateSidePanelToolbarName,
                         [](ContextualTasksWebView* web_view) -> views::View* {
                           return web_view->content_web_view();
                         }),
        InstrumentNonTabWebView(kTabStateSidePanelToolbarId,
                                kTabStateSidePanelToolbarName));
  }

  // Clicks `button_id` in the side panel toolbar.
  MultiStep ClickToolbarButton(const std::string& button_id) {
    const DeepQuery kButton = {IsRearchitectureEnabled()
                                   ? "contextual-tasks-toolbar-app"
                                   : "contextual-tasks-app",
                               "#toolbar", "#" + button_id};
    return Steps(
        InstrumentSidePanelToolbar(),
        WaitForElementExists(kTabStateSidePanelToolbarId, kButton),
        // The click can navigate or hide the panel, so don't wait for
        // a response.
        ExecuteJsAt(kTabStateSidePanelToolbarId, kButton, "el => el.click()",
                    ExecuteJsMode::kFireAndForget));
  }

  // Clicks the toolbar's New thread button on tab0 while tab0 and tab2 share a
  // thread, and verifies that only tab0 moves to a new thread.
  void RunNewThreadButtonTest() {
    content::WebContents* panel_contents = nullptr;
    RunTestSequence(
        OpenPanelWithSharedThread(),
        Do([&]() { panel_contents = GetPanelContents(); }),

        ClickToolbarButton("newThreadButton"),
        PollUntil(
            [&]() {
              std::optional<base::Uuid> task_id = GetTaskIdForTab(0);
              return task_id && *task_id != shared_task_id_;
            },
            "Wait for tab0 to move to a new thread."),
        Do([&]() {
          // The new thread starts in a new panel `WebContents` for tab0.
          content::WebContents* new_thread_contents = GetPanelContents();
          ASSERT_TRUE(new_thread_contents);
          EXPECT_TRUE(IsPanelOpen());
          EXPECT_NE(panel_contents, new_thread_contents);
          EXPECT_FALSE(ShowsThread(new_thread_contents));

          // tab2 keeps showing the existing thread in the original panel
          // `WebContents`.
          ActivateTab(2);
          EXPECT_TRUE(IsPanelOpen());
          EXPECT_EQ(shared_task_id_, GetTaskIdForTab(2));
          EXPECT_EQ(panel_contents, GetPanelContents());
          EXPECT_TRUE(ShowsThread(GetPanelContents()));

          // tab0 still shows the new thread.
          ActivateTab(0);
          EXPECT_TRUE(IsPanelOpen());
          EXPECT_EQ(new_thread_contents, GetPanelContents());
          EXPECT_NE(shared_task_id_, GetTaskIdForTab(0));
        }));
  }

  std::string entry_point_ = "toolbar-ephemeral-branded";
  bool tab_state_enabled_ = true;
  bool notify_zero_state_rendered_ = false;
  base::Uuid shared_task_id_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksSidePanelTabStateInteractiveUiTest,
                         testing::Bool(),
                         &RearchitectureParamName);

// This tests the following CUJ with the ephemeral toolbar button:
//  (1) The side panel is open with the same thread on tab0 and tab2.
//  (2) User closes the panel on tab0 from the toolbar's close button.
//  (3) The panel stays open with the thread on tab2, and closed on tab0.
//  (4) User reopens the panel on tab0, which resumes the thread.
IN_PROC_BROWSER_TEST_P(ContextualTasksSidePanelTabStateInteractiveUiTest,
                       CloseButton_KeepsPanelOpenOnTabsSharingThread) {
  content::WebContents* panel_contents = nullptr;
  RunTestSequence(
      OpenPanelWithSharedThread(),
      Do([&]() { panel_contents = GetPanelContents(); }),

      ClickToolbarButton("closeButton"),
      WaitForHide(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        EXPECT_FALSE(IsPanelOpen());
        // tab0 stays on the thread so that the ephemeral button can resume it.
        EXPECT_EQ(shared_task_id_, GetTaskIdForTab(0));

        ActivateTab(2);
        EXPECT_TRUE(IsPanelOpen());
        EXPECT_EQ(panel_contents, GetPanelContents());
        EXPECT_TRUE(ShowsThread(GetPanelContents()));

        // tab1 doesn't have a thread.
        ActivateTab(1);
        EXPECT_FALSE(IsPanelOpen());

        ActivateTab(0);
        EXPECT_FALSE(IsPanelOpen());

        // Switching between the tabs again keeps their state.
        ActivateTab(2);
        EXPECT_TRUE(IsPanelOpen());
        ActivateTab(0);
        EXPECT_FALSE(IsPanelOpen());

        // Reopen the panel on tab0 as the ephemeral toolbar button does.
        GetPanelController()->Show(
            /*transition_from_tab=*/false,
            omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
            /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
            ContextualTasksPanelController::EntrySource::
                kEphemeralToolbarButton);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        EXPECT_EQ(shared_task_id_, GetTaskIdForTab(0));
        EXPECT_EQ(panel_contents, GetPanelContents());
        EXPECT_TRUE(ShowsThread(GetPanelContents()));

        ActivateTab(2);
        EXPECT_TRUE(IsPanelOpen());
        EXPECT_EQ(panel_contents, GetPanelContents());
      }));
}

// This tests the following CUJ:
//  (1) The side panel is open with the same thread on tab0 and tab2.
//  (2) User closes the panel on tab0, and then closes tab0.
//  (3) The panel stays open with the thread on tab2.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksSidePanelTabStateInteractiveUiTest,
    ClosePanelThenCloseTab_KeepsPanelOpenOnTabsSharingThread) {
  content::WebContents* panel_contents = nullptr;
  RunTestSequence(OpenPanelWithSharedThread(), Do([&]() {
                    panel_contents = GetPanelContents();
                    GetPanelController()->Close();
                    EXPECT_FALSE(IsPanelOpen());

                    // Close tab0, so tab1 and tab2 move to indices 0 and 1.
                    browser()->tab_strip_model()->CloseWebContentsAt(
                        0, TabCloseTypes::CLOSE_USER_GESTURE);
                    ASSERT_EQ(2, browser()->tab_strip_model()->count());

                    ActivateTab(1);
                    EXPECT_EQ(shared_task_id_, GetTaskIdForTab(1));
                    EXPECT_TRUE(IsPanelOpen());
                    EXPECT_EQ(panel_contents, GetPanelContents());
                    EXPECT_TRUE(ShowsThread(GetPanelContents()));
                  }));
}

// This tests the following CUJ:
//  (1) The side panel is open with the same thread on tab0 and tab2.
//  (2) User clicks the toolbar's New thread button on tab0.
//  (3) tab0 moves to a new thread, and tab2 keeps showing the existing thread.
IN_PROC_BROWSER_TEST_P(ContextualTasksSidePanelTabStateInteractiveUiTest,
                       NewThreadButton_OnlyChangesThreadOnActiveTab) {
  RunNewThreadButtonTest();
}

// This tests the following CUJ in the legacy UI:
//  (1) The side panel is open with the same thread on tab0 and tab2.
//  (2) User switches tab0's panel to another thread from thread history.
//  (3) tab0 shows the other thread, and tab2 keeps showing the existing thread.
IN_PROC_BROWSER_TEST_P(ContextualTasksSidePanelTabStateInteractiveUiTest,
                       ThreadHistorySwitch_OnlyChangesThreadOnActiveTab) {
  if (IsRearchitectureEnabled()) {
    GTEST_SKIP() << "In the rearchitecture, thread history switches happen "
                    "inside the AI page without notifying the browser.";
  }
  // Add a thread from thread history that isn't open in any tab.
  ContextualTasksService* service = GetContextualTasksService();
  const base::Uuid history_task_id = service->CreateTask().GetTaskId();
  service->UpdateThreadForTask(history_task_id, ThreadType::kAiMode, "thread2",
                               std::nullopt, "Title 2");
  const GURL kHistoryThreadUrl(
      "https://www.google.com/search?udm=50&q=Title+2&mtid=thread2");
  const DeepQuery kThreadFrame = {"contextual-tasks-app", "#threadFrame"};
  content::WebContents* panel_contents = nullptr;
  RunTestSequence(
      OpenPanelWithSharedThread(),
      Do([&panel_contents, this]() { panel_contents = GetPanelContents(); }),
      InstrumentSidePanelToolbar(),

      // Switch tab0's panel to the thread from thread history.
      ExecuteJsAt(
          kTabStateSidePanelToolbarId, kThreadFrame,
          content::JsReplace("el => { el.src = $1; }", kHistoryThreadUrl)),
      PollUntil([&history_task_id,
                 this]() { return GetTaskIdForTab(0) == history_task_id; },
                "Wait for tab0 to switch threads."),
      Do([&panel_contents, this]() {
        EXPECT_EQ(panel_contents, GetPanelContents());
        std::vector<content::WebContents*> panels =
            GetPanelController()->GetPanelWebContentsList();
        ASSERT_EQ(2u, panels.size());
        content::WebContents* rebuilt_contents =
            panels[0] == panel_contents ? panels[1] : panels[0];

        // tab2 stays on the existing thread, in the panel WebContents
        // recreated when the thread switched.
        EXPECT_EQ(shared_task_id_, GetTaskIdForTab(2));
        ActivateTab(2);
        EXPECT_TRUE(IsPanelOpen());
        EXPECT_EQ(rebuilt_contents, GetPanelContents());
      }),
      PollUntil([this]() { return ShowsThread(GetPanelContents()); },
                "Wait for tab2's panel to show the existing thread."),
      Do([&panel_contents, &history_task_id, this]() {
        // tab0 still shows the thread from thread history.
        ActivateTab(0);
        EXPECT_TRUE(IsPanelOpen());
        EXPECT_EQ(panel_contents, GetPanelContents());
        EXPECT_EQ(history_task_id, GetTaskIdForTab(0));
      }));
}

// This tests the following CUJ:
//  (1) The side panel is open with a thread on tab0, the only tab on the
//      thread.
//  (2) User closes tab0, and then reopens it with Ctrl+Shift+T.
//  (3) The reopened tab shows the panel with the thread.
IN_PROC_BROWSER_TEST_P(ContextualTasksSidePanelTabStateInteractiveUiTest,
                       RestoreClosedTab_RestoresPanelWithThread) {
  RunTestSequence(
      OpenPanelWithThread(), CloseAndRestoreTab0(),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      PollUntil([this]() { return ShowsThread(GetPanelContents()); },
                "Wait for the thread to load in the reopened tab's panel."),
      Do([this]() {
        EXPECT_TRUE(IsPanelOpen());
        EXPECT_TRUE(GetTaskIdForTab(0).has_value());
      }));
}

// This tests the following CUJ:
//  (1) The side panel is open with the same thread on tab0 and tab2.
//  (2) User closes tab0, and then reopens it with Ctrl+Shift+T.
//  (3) The reopened tab shares the panel with tab2 again.
IN_PROC_BROWSER_TEST_P(ContextualTasksSidePanelTabStateInteractiveUiTest,
                       RestoreClosedTab_SharedThread_RejoinsPanel) {
  content::WebContents* panel_contents = nullptr;
  RunTestSequence(OpenPanelWithSharedThread(), Do([&panel_contents, this]() {
                    panel_contents = GetPanelContents();
                  }),
                  CloseAndRestoreTab0(),
                  WaitForShow(kContextualTasksSidePanelWebViewElementId),
                  Do([&panel_contents, this]() {
                    EXPECT_EQ(shared_task_id_, GetTaskIdForTab(0));
                    EXPECT_TRUE(IsPanelOpen());
                    EXPECT_EQ(panel_contents, GetPanelContents());
                  }));
}

// This tests the following CUJ:
//  (1) The side panel is open with a thread on tab0.
//  (2) User closes the panel, closes tab0, and then reopens it with
//      Ctrl+Shift+T.
//  (3) The reopened tab doesn't show the panel.
IN_PROC_BROWSER_TEST_P(ContextualTasksSidePanelTabStateInteractiveUiTest,
                       RestoreClosedTab_PanelClosed_DoesNotRestorePanel) {
  RunTestSequence(OpenPanelWithThread(), Do([this]() {
                    GetPanelController()->Close();
                    EXPECT_FALSE(IsPanelOpen());
                  }),
                  CloseAndRestoreTab0(), Do([this]() {
                    EXPECT_FALSE(IsPanelOpen());
                    EXPECT_FALSE(GetTaskIdForTab(0).has_value());
                  }));
}

class ContextualTasksSidePanelTabStateNoEntryPointInteractiveUiTest
    : public ContextualTasksSidePanelTabStateInteractiveUiTest {
 public:
  ContextualTasksSidePanelTabStateNoEntryPointInteractiveUiTest() {
    entry_point_ = "no-entry-point";
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksSidePanelTabStateNoEntryPointInteractiveUiTest,
    testing::Bool(),
    &RearchitectureParamName);

// This tests the following CUJ without the ephemeral toolbar button:
//  (1) The side panel is open with the same thread on tab0 and tab2.
//  (2) User closes the panel on tab0 from the toolbar's close button.
//  (3) The panel stays open with the thread on tab2, and closed on tab0.
//  (4) User reopens the panel on tab0, which starts a new thread.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksSidePanelTabStateNoEntryPointInteractiveUiTest,
    CloseButton_ReopeningStartsNewThreadOnlyOnActiveTab) {
  content::WebContents* panel_contents = nullptr;
  RunTestSequence(
      OpenPanelWithSharedThread(),
      Do([&]() { panel_contents = GetPanelContents(); }),

      ClickToolbarButton("closeButton"),
      WaitForHide(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        EXPECT_FALSE(IsPanelOpen());
        // tab0 leaves the thread, so reopening the panel starts a new one.
        EXPECT_FALSE(GetTaskIdForTab(0).has_value());

        ActivateTab(2);
        EXPECT_TRUE(IsPanelOpen());
        EXPECT_EQ(panel_contents, GetPanelContents());
        EXPECT_TRUE(ShowsThread(GetPanelContents()));

        ActivateTab(0);
        EXPECT_FALSE(IsPanelOpen());
        GetPanelController()->Show(
            /*transition_from_tab=*/false,
            omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        std::optional<base::Uuid> task_id = GetTaskIdForTab(0);
        ASSERT_TRUE(task_id.has_value());
        EXPECT_NE(shared_task_id_, *task_id);
        EXPECT_NE(panel_contents, GetPanelContents());

        // tab2 still shows the existing thread.
        ActivateTab(2);
        EXPECT_TRUE(IsPanelOpen());
        EXPECT_EQ(shared_task_id_, GetTaskIdForTab(2));
        EXPECT_EQ(panel_contents, GetPanelContents());
      }));
}

// With the AI page notifying the browser when its zero state is rendered, the
// legacy UI doesn't start a new task when the zero state loads, so the New
// thread button can't rely on that to move only the active tab.
class ContextualTasksSidePanelTabStateNotifyZeroStateInteractiveUiTest
    : public ContextualTasksSidePanelTabStateInteractiveUiTest {
 public:
  ContextualTasksSidePanelTabStateNotifyZeroStateInteractiveUiTest() {
    notify_zero_state_rendered_ = true;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksSidePanelTabStateNotifyZeroStateInteractiveUiTest,
    testing::Bool(),
    &RearchitectureParamName);

IN_PROC_BROWSER_TEST_P(
    ContextualTasksSidePanelTabStateNotifyZeroStateInteractiveUiTest,
    NewThreadButton_OnlyChangesThreadOnActiveTab) {
  RunNewThreadButtonTest();
}

// With side panel tab state disabled, closing the panel or changing its thread
// applies to all tabs that share the thread, and reopening a closed tab doesn't
// restore its panel.
class ContextualTasksSidePanelTabStateDisabledInteractiveUiTest
    : public ContextualTasksSidePanelTabStateInteractiveUiTest {
 public:
  ContextualTasksSidePanelTabStateDisabledInteractiveUiTest() {
    tab_state_enabled_ = false;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksSidePanelTabStateDisabledInteractiveUiTest,
    testing::Bool(),
    &RearchitectureParamName);

IN_PROC_BROWSER_TEST_P(
    ContextualTasksSidePanelTabStateDisabledInteractiveUiTest,
    CloseButton_ClosesPanelOnTabsSharingThread) {
  RunTestSequence(
      OpenPanelWithSharedThread(), ClickToolbarButton("closeButton"),
      WaitForHide(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        ActivateTab(2);
        EXPECT_FALSE(IsPanelOpen());
      }));
}

IN_PROC_BROWSER_TEST_P(
    ContextualTasksSidePanelTabStateDisabledInteractiveUiTest,
    NewThreadButton_ChangesThreadOnTabsSharingThread) {
  content::WebContents* panel_contents = nullptr;
  RunTestSequence(OpenPanelWithSharedThread(),
                  Do([&]() { panel_contents = GetPanelContents(); }),
                  ClickToolbarButton("newThreadButton"),
                  PollUntil([&]() { return !ShowsThread(panel_contents); },
                            "Wait for the new thread to load in the panel."),
                  Do([&]() {
                    // Both tabs show the new thread in the same panel
                    // `WebContents`.
                    EXPECT_EQ(GetTaskIdForTab(0), GetTaskIdForTab(2));
                    ActivateTab(2);
                    EXPECT_TRUE(IsPanelOpen());
                    EXPECT_EQ(panel_contents, GetPanelContents());
                    EXPECT_FALSE(ShowsThread(GetPanelContents()));
                  }));
}

IN_PROC_BROWSER_TEST_P(
    ContextualTasksSidePanelTabStateDisabledInteractiveUiTest,
    RestoreClosedTab_DoesNotRestorePanel) {
  RunTestSequence(OpenPanelWithThread(), CloseAndRestoreTab0(), Do([this]() {
                    EXPECT_FALSE(IsPanelOpen());
                    EXPECT_FALSE(GetTaskIdForTab(0).has_value());
                  }));
}

}  // namespace contextual_tasks
