// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/compose/buildflags.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/lens/buildflags.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/search_engines/template_url.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/vector2d.h"

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/chrome_compose_client.h"
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#endif

class AccessibilityLabelsMenuObserver;
class Browser;
class ClickToCallContextMenuObserver;
class LinkToTextMenuObserver;
class PrintPreviewContextMenuObserver;
class Profile;
class ReadWriteCardObserver;
class SpellingMenuObserver;
class SpellingOptionsSubMenuObserver;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
class MenuItem;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
class SystemWebAppDelegate;
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  lens::LensRegionSearchController* GetLensRegionSearchControllerForTesting() {
    return lens_region_search_controller_.get();
  }
#endif

 protected:
  Profile* GetProfile() const;

  // This may return nullptr (e.g. for WebUI dialogs). Virtual to allow tests to
  // override.
  virtual Browser* GetBrowser() const;

  // Returns the correct IDC for the Search by Image context menu string
  int GetSearchForImageIdc() const;

  // Returns the correct IDC for the Translate Image context menu string
  int GetTranslateImageIdc() const;

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
  // If called in Ash when Lacros is the only browser, this open the URL in
  // Lacros. In that case, only the |url| and some values of |disposition| are
  // respected - other parameters are ignored. The |initiator| parameter is the
  // origin that supplied the URL being navigated to; it may be an opaque origin
  // with no precursor if the URL came from the browser itself or the user.
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  static bool ExtensionContextAndPatternMatch(
      const content::ContextMenuParams& params,
      const extensions::MenuItem::ContextList& contexts,
      const extensions::URLPatternSet& target_url_patterns);
  static bool MenuItemMatchesParams(const content::ContextMenuParams& params,
                                    const extensions::MenuItem* item);
#endif

  // Formats a URL to be written to the clipboard and returns the formatted
  // string. Used by WriteURLToClipboard(), but kept in a separate function so
  // the formatting behavior can be tested without having to initialize the
  // clipboard. |url| must be valid and non-empty.
  static std::u16string FormatURLForClipboard(const GURL& url);

  // Writes the specified text/url to the system clipboard.
  void WriteURLToClipboard(const GURL& url);

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

  void AppendDeveloperItems();
  void AppendDevtoolsForUnpackedExtensions();
  void AppendLinkItems();
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
  void AppendReadingModeItem();
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
  void AppendAllExtensionItems();
  void AppendCurrentExtensionItems();
#endif
  void AppendPrintPreviewItems();
  void AppendSearchLensForImageItems();
  void AppendSearchWebForImageItems();
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
  // status. For context menu commands that make network requests, this check
  // should be applied.
  bool IsAllowedByUntrustedNetworkStatus() const;

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
  bool IsOpenLinkOTREnabled() const;
  bool IsOpenLinkAllowedByDlp(const GURL& link_url) const;
  bool IsRegionSearchEnabled() const;
  bool IsAddANoteEnabled() const;
  bool IsVideoFrameItemEnabled(int id) const;

  // Command execution functions.
  void ExecSearchWebInCompanionSidePanel(const GURL& url);
  void ExecSearchWebInSidePanel(const GURL& url);
  void ExecOpenWebApp();
  void ExecOpenLinkPreview();
  void ExecProtocolHandler(int event_flags, int handler_index);
  void ExecOpenLinkInProfile(int profile_index);
  void ExecInspectElement();
  void ExecInspectBackgroundPage();
  void ExecSaveLinkAs();
  void ExecSaveAs();
  void ExecExitFullscreen();
  void ExecCopyLinkText();
  void ExecCopyImageAt();
  void ExecSearchLensForImage(int event_flags, bool is_image_translate);
  void ExecAddANote();
  void ExecRegionSearch(int event_flags,
                        bool is_google_default_search_provider);
  void ExecSearchWebForImage(bool is_image_translate);
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
      supervised_user::FilteringBehavior filtering_behavior,
      supervised_user::FilteringBehaviorReason reason,
      bool uncertain);

  // Opens the Lens overlay to search a region defined by the given bounds of
  // the view and the image to be searched. Tab bounds and view bounds are
  // relative to the screen and in DP, while image bounds are relative to the
  // view and in physical pixels. The device scale factor is supplied to scale
  // the image bounds properly.
  void OpenLensOverlayWithPreselectedRegion(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The system app (if any) associated with the WebContents we're in.
  raw_ptr<const ash::SystemWebAppDelegate> system_app_ = nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // A one-time callback that will be called the next time a plugin action is
  // executed from a given render frame.
  ExecutePluginActionCallback execute_plugin_action_callback_;

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  // Controller for Lens Region Search feature. This controller will be
  // destroyed as soon as the RenderViewContextMenu object is destroyed. The
  // RenderViewContextMenu is reset every time it is shown, but persists between
  // uses so that it doesn't go out of scope before finishing work. This means
  // that when another context menu opens, the Lens Region Search feature will
  // close if active.
  std::unique_ptr<lens::LensRegionSearchController>
      lens_region_search_controller_;
#endif

  // Responsible for handling autofill related context menu items.
  autofill::AutofillContextMenuManager autofill_context_menu_manager_;

  base::WeakPtrFactory<RenderViewContextMenu> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
