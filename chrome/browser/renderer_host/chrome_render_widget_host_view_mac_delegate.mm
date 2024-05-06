// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_delegate.h"

#include <cmath>

#include "base/auto_reset.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/top_chrome/webui_url_utils.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

@interface ChromeRenderWidgetHostViewMacDelegate () <HistorySwiperDelegate>

@property(readonly) content::WebContents* webContents;
@property(readonly) NSView* nsView;
@property(readonly) PrefService* prefService;

@end

@implementation ChromeRenderWidgetHostViewMacDelegate {
  // The widget host (process + routing IDs) that this delegate is managing.
  int32_t _widgetProcessId;
  int32_t _widgetRoutingId;

  // Responsible for 2-finger swipes history navigation.
  HistorySwiper* __strong _historySwiper;

  // A boolean set to true while resigning first responder status, to avoid
  // infinite recursion in the case of reentrance.
  BOOL _resigningFirstResponder;
}

- (instancetype)initWithRenderWidgetHost:
    (content::RenderWidgetHost*)renderWidgetHost {
  self = [super init];
  if (self) {
    _widgetProcessId = renderWidgetHost->GetProcess()->GetID();
    _widgetRoutingId = renderWidgetHost->GetRoutingID();
    _historySwiper = [[HistorySwiper alloc] initWithDelegate:self];
  }
  return self;
}

- (void)dealloc {
  [_historySwiper setDelegate:nil];
}

- (content::WebContents*)webContents {
  content::RenderWidgetHost* renderWidgetHost =
      content::RenderWidgetHost::FromID(_widgetProcessId, _widgetRoutingId);
  if (!renderWidgetHost) {
    return nullptr;
  }

  content::RenderViewHost* renderViewHost =
      content::RenderViewHost::From(renderWidgetHost);
  if (!renderViewHost) {
    return nullptr;
  }

  return content::WebContents::FromRenderViewHost(renderViewHost);
}

- (NSView*)nsView {
  content::RenderWidgetHost* renderWidgetHost =
      content::RenderWidgetHost::FromID(_widgetProcessId, _widgetRoutingId);
  if (!renderWidgetHost) {
    return nil;
  }

  content::RenderWidgetHostView* renderWidgetHostView =
      renderWidgetHost->GetView();
  if (!renderWidgetHostView) {
    return nil;
  }

  return renderWidgetHostView->GetNativeView().GetNativeNSView();
}

- (PrefService*)prefService {
  content::RenderWidgetHost* renderWidgetHost =
      content::RenderWidgetHost::FromID(_widgetProcessId, _widgetRoutingId);
  if (!renderWidgetHost) {
    return nullptr;
  }

  return Profile::FromBrowserContext(
             renderWidgetHost->GetProcess()->GetBrowserContext())
      ->GetPrefs();
}

// Handle an event. All incoming key and mouse events flow through this
// delegate method if implemented. Return YES if the event is fully handled, or
// NO if normal processing should take place.
- (BOOL)handleEvent:(NSEvent*)event {
  return [_historySwiper handleEvent:event];
}

// NSWindow events.

- (void)beginGestureWithEvent:(NSEvent*)event {
  [_historySwiper beginGestureWithEvent:event];
}

- (void)endGestureWithEvent:(NSEvent*)event {
  [_historySwiper endGestureWithEvent:event];
}

// This is a low level API which provides touches associated with an event.
// It is used in conjunction with gestures to determine finger placement
// on the trackpad.
- (void)touchesMovedWithEvent:(NSEvent*)event {
  [_historySwiper touchesMovedWithEvent:event];
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  [_historySwiper touchesBeganWithEvent:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [_historySwiper touchesCancelledWithEvent:event];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  [_historySwiper touchesEndedWithEvent:event];
}

// HistorySwiperDelegate methods

- (BOOL)shouldAllowHistorySwiping {
  content::WebContents* webContents = self.webContents;
  if (!webContents) {
    return NO;
  }
  return !DevToolsWindow::IsDevToolsWindow(webContents);
}

- (NSView*)viewThatWantsHistoryOverlay {
  return self.nsView;
}

- (BOOL)canNavigateInDirection:(history_swiper::NavigationDirection)direction
                      onWindow:(NSWindow*)window {
  content::WebContents* webContents = self.webContents;
  if (!webContents) {
    return NO;
  }

  if (direction == history_swiper::kForwards) {
    return chrome::CanGoForward(webContents);
  } else {
    return chrome::CanGoBack(webContents);
  }
}

- (void)navigateInDirection:(history_swiper::NavigationDirection)direction
                   onWindow:(NSWindow*)window {
  content::WebContents* webContents = self.webContents;
  if (!webContents) {
    return;
  }

  if (direction == history_swiper::kForwards) {
    chrome::GoForward(webContents);
  } else {
    chrome::GoBack(webContents);
  }
}

- (void)backwardsSwipeNavigationLikely {
  content::WebContents* webContents = self.webContents;
  if (!webContents) {
    return;
  }

  webContents->BackNavigationLikely(
      content::preloading_predictor::kBackGestureNavigation,
      WindowOpenDisposition::CURRENT_TAB);
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                      isValidItem:(BOOL*)valid {
  PrefService* pref = self.prefService;
  if (!pref) {
    return NO;
  }

  const PrefService::Preference* spellCheckEnablePreference =
      pref->FindPreference(spellcheck::prefs::kSpellCheckEnable);
  DCHECK(spellCheckEnablePreference);
  const bool spellCheckUserModifiable =
      spellCheckEnablePreference->IsUserModifiable();

  SEL action = item.action;
  // For now, this action is always enabled for render view;
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(checkSpelling:)) {
    *valid = spellCheckUserModifiable;
    return YES;
  }

  // TODO(groby): Clarify who sends this and if toggleContinuousSpellChecking:
  // is still necessary.
  if (action == @selector(toggleContinuousSpellChecking:)) {
    if ([(id)item respondsToSelector:@selector(setState:)]) {
      NSControlStateValue checkedState =
          pref->GetBoolean(spellcheck::prefs::kSpellCheckEnable)
              ? NSControlStateValueOn
              : NSControlStateValueOff;
      [(id)item setState:checkedState];
    }
    *valid = spellCheckUserModifiable;
    return YES;
  }

  if (action == @selector(showGuessPanel:) ||
      action == @selector(toggleGrammarChecking:)) {
    *valid = spellCheckUserModifiable;
    return YES;
  }

  return NO;
}

- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed {
  [_historySwiper rendererHandledWheelEvent:event consumed:consumed];
}

- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed {
  [_historySwiper rendererHandledGestureScrollEvent:event consumed:consumed];
}

- (void)rendererHandledOverscrollEvent:(const ui::DidOverscrollParams&)params {
  [_historySwiper onOverscrolled:params];
}

// Spellchecking methods
// The next five methods are implemented here since this class is the first
// responder for anything in the browser.

// This message is sent whenever the user specifies that a word should be
// changed from the spellChecker.
- (void)changeSpelling:(id)sender {
  content::WebContents* webContents = self.webContents;
  if (!webContents) {
    return;
  }

  // Grab the currently selected word from the spell panel, as this is the word
  // that we want to replace the selected word in the text with.
  NSString* newWord = [[sender selectedCell] stringValue];
  if (newWord != nil) {
    webContents->ReplaceMisspelling(base::SysNSStringToUTF16(newWord));
  }
}

// This message is sent by NSSpellChecker whenever the next word should be
// advanced to, either after a correction or clicking the "Find Next" button.
// This isn't documented anywhere useful, like in NSSpellProtocol.h with the
// other spelling panel methods. This is probably because Apple assumes that the
// the spelling panel will be used with an NSText, which will automatically
// catch this and advance to the next word for you. Thanks Apple.
// This is also called from the Edit -> Spelling -> Check Spelling menu item.
- (void)checkSpelling:(id)sender {
  content::WebContents* webContents = self.webContents;
  if (!webContents) {
    return;
  }

  if (content::RenderFrameHost* frame = webContents->GetFocusedFrame()) {
    mojo::Remote<spellcheck::mojom::SpellCheckPanel>
        focused_spell_check_panel_client;
    frame->GetRemoteInterfaces()->GetInterface(
        focused_spell_check_panel_client.BindNewPipeAndPassReceiver());
    focused_spell_check_panel_client->AdvanceToNextMisspelling();
  }
}

// This message is sent by the spelling panel whenever a word is ignored.
- (void)ignoreSpelling:(id)sender {
  // Ideally, we would ask the current RenderView for its tag, but that would
  // mean making a blocking IPC call from the browser. Instead,
  // spellcheck_platform::CheckSpelling remembers the last tag and
  // spellcheck_platform::IgnoreWord assumes that is the correct tag.
  NSString* wordToIgnore = [sender stringValue];
  if (wordToIgnore != nil)
    spellcheck_platform::IgnoreWord(nullptr,
                                    base::SysNSStringToUTF16(wordToIgnore));
}

- (void)showGuessPanel:(id)sender {
  content::WebContents* webContents = self.webContents;
  if (!webContents) {
    return;
  }

  const bool visible = spellcheck_platform::SpellingPanelVisible();

  if (content::RenderFrameHost* frame = webContents->GetFocusedFrame()) {
    mojo::Remote<spellcheck::mojom::SpellCheckPanel>
        focused_spell_check_panel_client;
    frame->GetRemoteInterfaces()->GetInterface(
        focused_spell_check_panel_client.BindNewPipeAndPassReceiver());
    focused_spell_check_panel_client->ToggleSpellPanel(visible);
  }
}

- (void)toggleContinuousSpellChecking:(id)sender {
  PrefService* pref = self.prefService;
  if (!pref) {
    return;
  }
  pref->SetBoolean(spellcheck::prefs::kSpellCheckEnable,
                   !pref->GetBoolean(spellcheck::prefs::kSpellCheckEnable));
}

// END Spellchecking methods

// If a dialog is visible, make its window key. See becomeFirstResponder.
- (void)makeAnyDialogKey {
  content::WebContents* webContents = self.webContents;
  if (!webContents) {
    return;
  }

  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(webContents);
  if (!manager) {
    return;
  }

  if (manager->IsDialogActive()) {
    manager->FocusTopmostDialog();
  }
}

// If the RenderWidgetHostView becomes first responder while it has a dialog
// (say, if the user was interacting with the omnibox and then tabs back into
// the web contents), then make the dialog window key.
- (void)becomeFirstResponder {
  [self makeAnyDialogKey];
}

// If the RenderWidgetHostView is asked to resign first responder while a child
// window is key, then the user performed some action which targets the browser
// window, like clicking the omnibox or typing cmd+L. In that case, the browser
// window should become key.
- (void)resignFirstResponder {
  NSWindow* browserWindow = self.nsView.window;
  DCHECK(browserWindow);

  // If the browser window is already key, there's nothing to do.
  if (browserWindow.isKeyWindow) {
    return;
  }

  // Otherwise, look for it in the key window's chain of parents.
  NSWindow* keyWindowOrParent = NSApp.keyWindow;
  while (keyWindowOrParent && keyWindowOrParent != browserWindow) {
    keyWindowOrParent = keyWindowOrParent.parentWindow;
  }

  // If the browser window isn't among the parents, there's nothing to do.
  if (keyWindowOrParent != browserWindow) {
    return;
  }

  // Otherwise, temporarily set an ivar so that -windowDidBecomeKey, below,
  // doesn't immediately make the dialog key.
  base::AutoReset<BOOL> scoped(&_resigningFirstResponder, YES);

  // …then make the browser window key.
  [browserWindow makeKeyWindow];
}

// If the browser window becomes key while the RenderWidgetHostView is first
// responder, make the dialog key (if there is one).
- (void)windowDidBecomeKey {
  if (_resigningFirstResponder) {
    return;
  }
  NSView* view = self.nsView;
  if (view.window.firstResponder == view) {
    [self makeAnyDialogKey];
  }
}

- (AcceptMouseEventsOption)acceptsMouseEventsOption {
  content::WebContents* webContents = self.webContents;
  if (!webContents) {
    return kAcceptMouseEventsInActiveWindow;
  }

  // For Top Chrome WebUIs, allows inactive windows to accept
  // mouse events as long as the application is active. This
  // mimics the behavior of views UI.
  if (IsTopChromeWebUIURL(webContents->GetVisibleURL()) ||
      IsTopChromeUntrustedWebUIURL(webContents->GetVisibleURL())) {
    return kAcceptMouseEventsInActiveApp;
  }

  return kAcceptMouseEventsInActiveWindow;
}

@end
