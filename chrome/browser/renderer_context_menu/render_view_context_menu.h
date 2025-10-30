// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/compose/buildflags.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/search_engines/template_url.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/menus/simple_menu_model.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#endif

class AccessibilityLabelsMenuObserver;
class Browser;
#if BUILDFLAG(ENABLE_COMPOSE)
class ChromeComposeClient;
#endif
class ClickToCallContextMenuObserver;
class LinkToTextMenuObserver;
class PrintPreviewContextMenuObserver;
class Profile;
class ReadWriteCardObserver;
class SpellingMenuObserver;
class SpellingOptionsSubMenuObserver;
class ToastController;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}  // namespace extensions

namespace gfx {
class Point;
}

namespace blink {
namespace mojom {
class MediaPlayerAction;
}
}  // namespace blink

namespace ui {
class DataTransferEndpoint;
}

#if BUILDFLAG(IS_CHROMEOS)
namespace ash {
class SystemWebAppDelegate;
}

namespace chromeos::clipboard_history {
class ClipboardHistorySubmenuModel;
}  // namespace chromeos::clipboard_history

namespace policy {
class DlpRulesManager;
}  // namespace policy
#endif

class RenderViewContextMenu
    : public RenderViewContextMenuBase,
      public custom_handlers::ProtocolHandlerRegistry::Observer {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kExitFullscreenMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kComposeMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kGlicCloseMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kGlicReloadMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kGlicShareImageMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOpenLinkInSplitMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRegionSearchItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSearchForImageItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSearchForVideoFrameItem);

  using ExecutePluginActionCallback =
      base::OnceCallback<void(content::RenderFrameHost*,
                              blink::mojom::PluginActionType)>;

  RenderViewContextMenu(content::RenderFrameHost& render_frame_host,
                        const content::ContextMenuParams& params);

  RenderViewContextMenu(const RenderViewContextMenu&) = delete;
  RenderViewContextMenu& operator=(const RenderViewContextMenu&) = delete;

  ~RenderViewContextMenu() override;

  // Adds the spell check service item to the context menu.
  static void AddSpellCheckServiceItem(ui::SimpleMenuModel* menu,
                                       bool is_checked);

  // RenderViewContextMenuBase:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void AddSpellCheckServiceItem(bool is_checked) override;
  void AddAccessibilityLabelsServiceItem(bool is_checked) override;

  // Registers a one-time callback that will be called the next time a context
  // menu is shown.
  static void RegisterMenuShownCallbackForTesting(
      base::OnceCallback<void(RenderViewContextMenu*)> cb);

  // Register a one-time callback that will be called the next time a plugin
  // action is executed from a given render frame.
  void RegisterExecutePluginActionCallbackForTesting(
      base::OnceCallback<void(content::RenderFrameHost*,
                              blink::mojom::PluginActionType)> cb);

  bool lens_region_search_controller_started_for_testing() const {
    return lens_region_search_controller_started_for_testing_;
  }

  void AddObserverForTesting(RenderViewContextMenuObserver* observer);
  void RemoveObserverForTesting(RenderViewContextMenuObserver* observer);

 protected:
  Profile* GetProfile() const;

  // This may return nullptr (e.g. for WebUI dialogs). Virtual to allow tests to
  // override.
  virtual Browser* GetBrowser() const;

  // May return nullptr if the WebContents does not have an associated
  // BrowserWindowInterface (e.g. in isolated WebUI, or in tests).
  ToastController* GetToastController() const;

  // Returns the correct IDC for the Search by Image context menu string
  int GetSearchForImageIdc() const;

  // Returns the correct IDC for the Region Search context menu string
  int GetRegionSearchIdc() const;

  // Returns the correct IDC for the Video Frame Search context menu string
  int GetSearchForVideoFrameIdc() const;

  // Returns the provider for image search.
  const TemplateURL* GetImageSearchProvider() const;

  // Returns the correct provider name for the Search by Image context menu
  // string
  std::u16string GetImageSearchProviderName(const TemplateURL* provider) const;

  // Returns a (possibly truncated) version of the current selection text
  // suitable for putting in the title of a menu item.
  std::u16string PrintableSelectionText();

  // Helper function to escape "&" as "&&".
  void EscapeAmpersands(std::u16string* text);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ContextMenuMatcher extension_items_;
#endif
  void RecordUsedItem(int id) override;

  // Returns true if the browser is in HTML fullscreen mode, initiated by the
  // page (as opposed to the user). Used to determine which shortcut to display.
  bool IsHTML5Fullscreen() const;

  // Returns true if keyboard lock is active and requires the user to press and
  // hold escape to exit exclusive access mode.
  bool IsPressAndHoldEscRequiredToExitFullscreen() const;

#if BUILDFLAG(IS_CHROMEOS)
  virtual const policy::DlpRulesManager* GetDlpRulesManager() const;
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
  virtual ChromeComposeClient* GetChromeComposeClient() const;
#endif

  // RenderViewContextMenuBase:
  // The |initiator| parameter is the origin that supplied the URL being
  // navigated to; it may be an opaque origin with no precursor if the URL came
  // from the browser itself or the user.
  void OpenURLWithExtraHeaders(const GURL& url,
                               const GURL& referring_url,
                               const url::Origin& initiator,
                               WindowOpenDisposition disposition,
                               ui::PageTransition transition,
                               const std::string& extra_headers,
                               bool started_from_context_menu) override;

 private:
  friend class RenderViewContextMenuTest;
  friend class TestRenderViewContextMenu;
  friend class FormatUrlForClipboardTest;

  static bool IsDevToolsURL(const GURL& url);

  // Returns true if the command id is gated by fenced frame untrusted network
  // status.
  static bool IsCommandGatedByFencedFrameUntrustedNetworkStatus(int id);

  // Formats a URL to be written to the clipboard and returns the formatted
  // string. Used by WriteURLToClipboard(), but kept in a separate function so
  // the formatting behavior can be tested without having to initialize the
  // clipboard. |url| must be valid and non-empty.
  static std::u16string FormatURLForClipboard(const GURL& url);

  // Writes the specified url to the system clipboard.
  void WriteURLToClipboard(const GURL& url, int id);

  // Issues a preconnection request to the given url.
  void IssuePreconnectionToUrl(const std::string& anonymization_key_url,
                               const std::string& preconnect_url);

  // RenderViewContextMenuBase:
  void InitMenu() override;
  void RecordShownItem(int id, bool is_submenu) override;
#if BUILDFLAG(ENABLE_PLUGINS)
  void HandleAuthorizeAllPlugins() override;
#endif
  void NotifyMenuShown() override;

  // Returns whether the feature is new and should be shown with a "new" badge.
  //
  // When generating context menu items, we want to show a "new" badge next to
  // the item if the feature is new. Some of these items are generated
  // by this base class, and there we don't have direct access to the user
  // education service. Instead, we need to delegate to the platform-specific
  // implementation of this method to determine if the item should be marked
  // as "new".
  // This method accepts the feature name, and not the base::Feature.
  // The reason is that in DevTools, we don't have access to base::Features
  // directly, so features are stored by name and will be mapped accordingly.
  ui::IsNewFeatureAtValue GetIsNewFeatureAtValue(
      const std::string& feature_name) const override;

  // Gets the extension (if any) associated with the WebContents that we're in.
  const extensions::Extension* GetExtension() const;

  // Queries the Translate service to obtain the user's Translate target
  // language and returns the language name in its same locale.
  // On an already translated page, full page translation uses the current page
  // language as the target language while partial translation uses the last
  // used target language. |is_full_page_translation| controls the desired
  // outcome.
  std::u16string GetTargetLanguageDisplayName(
      bool is_full_page_translation) const;

  bool IsInProgressiveWebApp() const;
  bool IsLinkToIsolatedWebApp() const;

  void AppendDeveloperItems();
  void AppendDevtoolsForUnpackedExtensions();
  void AppendLinkItems();
  void AppendCopyLinkLocationItem();
  void AppendOpenWithLinkItems();
  void AppendSmartSelectionActionItems();
  void AppendOpenInWebAppLinkItems();
  void AppendReadWriteCardItems();
  void AppendImageItems();
  void AppendAudioItems();
  void AppendCanvasItems();
  void AppendVideoItems();
  void AppendMediaItems();
  void AppendPluginItems();
  void AppendPageItems();
  void AppendExitFullscreenItem();
  void AppendCopyItem();
  void AppendLinkToTextItems();
  void AppendPrintItem();
  void AppendPartialTranslateItem();
  void AppendTranslateItem();
  void AppendMediaRouterItem();
  void AppendReadAnythingItem();
  void AppendGlicItems();
  void AppendRotationItems();
  void AppendSpellingAndSearchSuggestionItems();
  void AppendOtherEditableItems();
  void AppendLanguageSettings();
  void AppendSpellingSuggestionItems();
  // Returns true if the items were appended. This might not happen in all
  // cases, e.g. these are only appended if a screen reader is enabled.
  bool AppendAccessibilityLabelsItems();
  void AppendSearchProvider();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  void AppendCurrentExtensionItems();
#endif
  void AppendPrintPreviewItems();
  void AppendSearchWebForImageItems();
  void AppendGlicShareImageItem();
  void AppendProtocolHandlerSubMenu();
  // TODO(b/316143236): Remove this method (along with the methods called by it)
  // once `kPasswordManualFallbackAvailable` is rolled out.
  void AppendPasswordItems();
  void AppendSharingItems();
  void AppendClickToCallItem();
  void AppendRegionSearchItem();
  void AppendLiveCaptionItem();
  void AppendSendTabToSelfItem(bool add_separator);
  void AppendUserNotesItems();
  bool AppendQRCodeGeneratorItem(bool for_image,
                                 bool draw_icon,
                                 bool add_separator);

  std::unique_ptr<ui::DataTransferEndpoint> CreateDataEndpoint(
      bool notify_if_restricted) const;

  // Helper function for checking policies.
  bool IsSaveAsItemAllowedByPolicy(const GURL& item_url) const;

  // Helper function for checking fenced frame tree untrusted network access
  // status. For context menu commands that are gated on fenced frame untrusted
  // network status, this check should be applied.
  bool IsUntrustedNetworkDisabled() const;

  // Helper function for checking if text query should be opened in Lens. Checks
  // whether Lens is available and whether the text selection entrypoint flag is
  // enabled.
  bool ShouldOpenTextQueryInLens() const;

  // Command enabled query functions.
  bool IsReloadEnabled() const;
  bool IsViewSourceEnabled() const;
  bool IsDevCommandEnabled(int id) const;
  bool IsTranslateEnabled() const;
  bool IsSaveLinkAsEnabled() const;
  bool IsSaveImageAsEnabled() const;
  bool IsSaveAsEnabled() const;
  bool IsSavePageEnabled() const;
  bool IsPasteEnabled() const;
  bool IsPasteAndMatchStyleEnabled() const;
  bool IsPrintPreviewEnabled() const;
  bool IsQRCodeGeneratorEnabled() const;
  bool IsRouteMediaEnabled() const;
  bool IsOpenLinkAllowedByDlp(const GURL& link_url) const;
  bool IsRegionSearchEnabled() const;
  bool IsAddANoteEnabled() const;
  bool IsVideoFrameItemEnabled(int id) const;

  // Command execution functions.
  void ExecOpenWebApp();
  void ExecOpenLinkPreview();
  void ExecProtocolHandler(int event_flags, int handler_index);
  void ExecOpenLinkInProfile(int profile_index);
  void ExecInspectElement();
  void ExecInspectBackgroundPage();
  void ExecSaveLinkAs();
  void ExecSaveAs();
  void ExecGlicShareImage();
  void ExecExitFullscreen();
  void ExecCopyLinkText();
  void ExecCopyImageAt();
  void ExecSearchLensForImage(int event_flags);
  void ExecAddANote();
  void ExecRegionSearch(int event_flags,
                        bool is_google_default_search_provider);
  void ExecSearchWebForImage();
  void ExecLoadImage();
  void ExecLoop();
  void ExecControls();
  void ExecSaveVideoFrameAs();
  void ExecCopyVideoFrame();
  void ExecSearchForVideoFrame(int event_flags, bool is_lens_query);
  void ExecLiveCaption();
  void ExecRotateCW();
  void ExecRotateCCW();
  void ExecReloadPackagedApp();
  void ExecRestartPackagedApp();
  void ExecPrint();
  void ExecRouteMedia();
  void ExecTranslate();
  void ExecPartialTranslate();
  void ExecLanguageSettings(int event_flags);
  void ExecProtocolHandlerSettings(int event_flags);
  void ExecPictureInPicture();
#if BUILDFLAG(ENABLE_COMPOSE)
  void ExecOpenCompose();
#endif
  void ExecOpenInReadAnything();

  void MediaPlayerAction(const blink::mojom::MediaPlayerAction& action);
  void SearchForVideoFrame(int event_flags,
                           bool is_lens_query,
                           const SkBitmap& bitmap,
                           const gfx::Rect& region_bounds);
  void PluginActionAt(const gfx::Point& location,
                      blink::mojom::PluginActionType plugin_action);
  void OpenTextQueryInLens();

  // Returns a list of registered ProtocolHandlers that can handle the clicked
  // on URL.
  custom_handlers::ProtocolHandlerRegistry::ProtocolHandlerList
  GetHandlersForLinkUrl();

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override;

  // Whether or not translation on this page can be triggered. This method
  // checks multiple criteria, e.g. whether translation is disabled by a policy
  // or whether the current page can be translated.
  bool CanTranslate(bool menu_logging);

  // Whether or not partial translation is supported for the current target
  // language.
  bool CanPartiallyTranslateTargetLanguage();

  // Under the correct conditions, issues a preconnection to the Lens URL and
  // warms up a renderer process.
  void MaybePrepareForLensQuery();

  // Does not execute "Save link as" if the URL is blocked by the URL filter.
  void CheckSupervisedUserURLFilterAndSaveLinkAs();
  void OnSupervisedUserURLFilterChecked(
      supervised_user::SupervisedUserURLFilter::Result result);

  // Opens the Lens overlay to search a region defined by the given bounds of
  // the view and the image to be searched. Tab bounds and view bounds are
  // relative to the screen and in DP, while image bounds are relative to the
  // view and in physical pixels. The device scale factor is supplied to scale
  // the image bounds properly.
  void OpenLensOverlayWithPreselectedRegion(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      lens::LensOverlayInvocationSource invocation_source,
      const gfx::Rect& tab_bounds,
      const gfx::Rect& view_bounds,
      float device_scale_factor,
      const SkBitmap& region_bytes,
      const gfx::Rect& region_bitmap);

#if BUILDFLAG(IS_CHROMEOS)
  // Shows the standalone clipboard history menu. `event_flags` describes the
  // event that caused the menu to show.
  void ShowClipboardHistoryMenu(int event_flags);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  // Opens the link in a new split view so that the linked page will be visible
  // next to the active tab. If the active tab is already in the split view,
  // then the tab that wasn't the source of the link will be navigated to the
  // link instead.
  void OpenLinkInSplitView();
#endif  // !BUILDFLAG(IS_ANDROID)

  // The destination URL to use if the user tries to search for or navigate to
  // a text selection.
  GURL selection_navigation_url_;

  // URL of current page and current main frame url
  GURL current_url_;
  GURL main_frame_url_;

  ui::SimpleMenuModel profile_link_submenu_model_;
  std::vector<base::FilePath> profile_link_paths_;
  bool multiple_profiles_open_;

  // Protocol handling:
  // - The submenu containing the installed protocol handlers.
  ui::SimpleMenuModel protocol_handler_submenu_model_;
  // - The registry with the protocols.
  raw_ptr<custom_handlers::ProtocolHandlerRegistry, DanglingUntriaged>
      protocol_handler_registry_;
  // - The observation of the registry.
  base::ScopedObservation<custom_handlers::ProtocolHandlerRegistry,
                          custom_handlers::ProtocolHandlerRegistry::Observer>
      protocol_handler_registry_observation_{this};
  // - Whether or not the registered protocols have changed since the menu was
  //   built.
  bool is_protocol_submenu_valid_ = false;

  // An observer that handles spelling suggestions, "Add to dictionary", and
  // "Use enhanced spell check" items.
  std::unique_ptr<SpellingMenuObserver> spelling_suggestions_menu_observer_;

  // An observer that handles accessibility labels items.
  std::unique_ptr<AccessibilityLabelsMenuObserver>
      accessibility_labels_menu_observer_;
  ui::SimpleMenuModel accessibility_labels_submenu_model_;

#if !BUILDFLAG(IS_MAC)
  // An observer that handles the submenu for showing spelling options. This
  // submenu lets users select the spelling language, for example.
  std::unique_ptr<SpellingOptionsSubMenuObserver>
      spelling_options_submenu_observer_;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // An observer that handles "Open with <app>" items.
  std::unique_ptr<RenderViewContextMenuObserver> open_with_menu_observer_;
  // An observer that handles smart text selection action items.
  std::unique_ptr<RenderViewContextMenuObserver>
      start_smart_selection_action_menu_observer_;
  // An observer that populates events to read write cards.
  std::unique_ptr<ReadWriteCardObserver> read_write_card_observer_;

  // A submenu model to contain clipboard history item descriptors. Used only if
  // the clipboard history refresh feature is enabled.
  std::unique_ptr<chromeos::clipboard_history::ClipboardHistorySubmenuModel>
      submenu_model_;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // An observer that disables menu items when print preview is active.
  std::unique_ptr<PrintPreviewContextMenuObserver> print_preview_menu_observer_;
#endif

  std::unique_ptr<LinkToTextMenuObserver> link_to_text_menu_observer_;

  // In the case of a MimeHandlerView this will point to the WebContents that
  // embeds the MimeHandlerViewGuest. Otherwise this will be the same as
  // |source_web_contents_|.
  const raw_ptr<content::WebContents, DanglingUntriaged> embedder_web_contents_;

  // Click to call menu observer.
  std::unique_ptr<ClickToCallContextMenuObserver>
      click_to_call_context_menu_observer_;

#if BUILDFLAG(IS_CHROMEOS)
  // The system app (if any) associated with the WebContents we're in.
  raw_ptr<const ash::SystemWebAppDelegate> system_app_ = nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // A one-time callback that will be called the next time a plugin action is
  // executed from a given render frame.
  ExecutePluginActionCallback execute_plugin_action_callback_;

  // Used in testing to determine whether the lens region search controller has
  // started due to interaction with the region search entrypoint in the menu.
  bool lens_region_search_controller_started_for_testing_ = false;

  // Responsible for handling autofill related context menu items.
  autofill::AutofillContextMenuManager autofill_context_menu_manager_;

  // Fenced frame can disable its untrusted network in exchange for access to
  // unpartitioned cross-site data. To prevent cross-site data from leaking out
  // of fenced frame, context menu commands should be gated on untrusted network
  // status if:
  // 1. It can be executed within a fenced frame.
  // 2. It can transfer information out of fenced frame. Network request is the
  // primary concern.
  //
  // See:
  // https://github.com/WICG/fenced-frame/blob/master/explainer/fenced_frames_with_local_unpartitioned_data_access.md#revoking-network-access

  // Note: Add `NO_IFTTT=<reason>` in the CL description if the linter is not
  // applicable. For example, if a new command that is not gated on fenced frame
  // network status is added, the following look up table does not require any
  // change.
  //
  // LINT.IfChange(CommandsGatedOnFencedFrameUntrustedNetworkStatus)
  static constexpr auto kFencedFrameUntrustedNetworkStatusGatedCommands =
      base::MakeFixedFlatSet<int>(
          {// For opening a link.
           IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
           IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
           IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
           IDC_OPEN_LINK_IN_PROFILE_FIRST, IDC_OPEN_LINK_IN_PROFILE_LAST,
           IDC_CONTENT_CONTEXT_OPENLINKSPLITVIEW,

           // Open link commands that appear in certain scenarios.
           IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP,
           IDC_CONTENT_CONTEXT_OPENLINKINPROFILE, IDC_CONTENT_CONTEXT_GOTOURL,
           IDC_CONTENT_CONTEXT_OPENLINKWITH, IDC_CONTENT_CONTEXT_OPENAVNEWTAB,
           IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB,

           // Link preview feature.
           IDC_CONTENT_CONTEXT_OPENLINKPREVIEW,

           // Image loading commands.
           IDC_CONTENT_CONTEXT_LOAD_IMAGE,
           IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB});
  // LINT.ThenChange(//chrome/app/chrome_command_ids.h:ChromeCommandIds)

  base::WeakPtrFactory<RenderViewContextMenu> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
