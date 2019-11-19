// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

using content::RenderViewHost;

@interface ChromeRenderWidgetHostViewMacDelegate () <HistorySwiperDelegate>
@end

@implementation ChromeRenderWidgetHostViewMacDelegate {
  BOOL resigningFirstResponder_;
}

- (id)initWithRenderWidgetHost:(content::RenderWidgetHost*)renderWidgetHost {
  self = [super init];
  if (self) {
    renderWidgetHost_ = renderWidgetHost;
    historySwiper_.reset([[HistorySwiper alloc] initWithDelegate:self]);
  }
  return self;
}

- (void)dealloc {
  [historySwiper_ setDelegate:nil];
  [super dealloc];
}

// Handle an event. All incoming key and mouse events flow through this
// delegate method if implemented. Return YES if the event is fully handled, or
// NO if normal processing should take place.
- (BOOL)handleEvent:(NSEvent*)event {
  return [historySwiper_ handleEvent:event];
}

// NSWindow events.

- (void)beginGestureWithEvent:(NSEvent*)event {
  [historySwiper_ beginGestureWithEvent:event];
}

- (void)endGestureWithEvent:(NSEvent*)event {
  [historySwiper_ endGestureWithEvent:event];
}

// This is a low level API which provides touches associated with an event.
// It is used in conjunction with gestures to determine finger placement
// on the trackpad.
- (void)touchesMovedWithEvent:(NSEvent*)event {
  [historySwiper_ touchesMovedWithEvent:event];
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  [historySwiper_ touchesBeganWithEvent:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [historySwiper_ touchesCancelledWithEvent:event];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  [historySwiper_ touchesEndedWithEvent:event];
}

// HistorySwiperDelegate methods

- (BOOL)shouldAllowHistorySwiping {
  if (!renderWidgetHost_)
    return NO;
  RenderViewHost* renderViewHost = RenderViewHost::From(renderWidgetHost_);
  if (!renderViewHost)
    return NO;
  content::WebContents* webContents =
      content::WebContents::FromRenderViewHost(renderViewHost);
  if (webContents && DevToolsWindow::IsDevToolsWindow(webContents)) {
    return NO;
  }

  return YES;
}

- (NSView*)viewThatWantsHistoryOverlay {
  return renderWidgetHost_->GetView()->GetNativeView().GetNativeNSView();
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                      isValidItem:(BOOL*)valid {
  SEL action = [item action];

  Profile* profile = Profile::FromBrowserContext(
      renderWidgetHost_->GetProcess()->GetBrowserContext());
  DCHECK(profile);
  PrefService* pref = profile->GetPrefs();
  const PrefService::Preference* spellCheckEnablePreference =
      pref->FindPreference(spellcheck::prefs::kSpellCheckEnable);
  DCHECK(spellCheckEnablePreference);
  const bool spellCheckUserModifiable =
      spellCheckEnablePreference->IsUserModifiable();

  // For now, this action is always enabled for render view;
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(checkSpelling:)) {
    *valid = spellCheckUserModifiable &&
             (RenderViewHost::From(renderWidgetHost_) != nullptr);
    return YES;
  }

  // TODO(groby): Clarify who sends this and if toggleContinuousSpellChecking:
  // is still necessary.
  if (action == @selector(toggleContinuousSpellChecking:)) {
    if ([(id)item respondsToSelector:@selector(setState:)]) {
      NSCellStateValue checkedState =
          pref->GetBoolean(spellcheck::prefs::kSpellCheckEnable) ? NSOnState
                                                                 : NSOffState;
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
  [historySwiper_ rendererHandledWheelEvent:event consumed:consumed];
}

- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed {
  [historySwiper_ rendererHandledGestureScrollEvent:event consumed:consumed];
}

- (void)rendererHandledOverscrollEvent:(const ui::DidOverscrollParams&)params {
  [historySwiper_ onOverscrolled:params];
}

// Spellchecking methods
// The next five methods are implemented here since this class is the first
// responder for anything in the browser.

// This message is sent whenever the user specifies that a word should be
// changed from the spellChecker.
- (void)changeSpelling:(id)sender {
  // Grab the currently selected word from the spell panel, as this is the word
  // that we want to replace the selected word in the text with.
  NSString* newWord = [[sender selectedCell] stringValue];
  if (newWord != nil) {
    content::WebContents* webContents =
        content::WebContents::FromRenderViewHost(
            RenderViewHost::From(renderWidgetHost_));
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
  content::WebContents* webContents = content::WebContents::FromRenderViewHost(
      RenderViewHost::From(renderWidgetHost_));
  DCHECK(webContents && webContents->GetFocusedFrame());

  mojo::Remote<spellcheck::mojom::SpellCheckPanel>
      focused_spell_check_panel_client;
  webContents->GetFocusedFrame()->GetRemoteInterfaces()->GetInterface(
      focused_spell_check_panel_client.BindNewPipeAndPassReceiver());
  focused_spell_check_panel_client->AdvanceToNextMisspelling();
}

// This message is sent by the spelling panel whenever a word is ignored.
- (void)ignoreSpelling:(id)sender {
  // Ideally, we would ask the current RenderView for its tag, but that would
  // mean making a blocking IPC call from the browser. Instead,
  // spellcheck_platform::CheckSpelling remembers the last tag and
  // spellcheck_platform::IgnoreWord assumes that is the correct tag.
  NSString* wordToIgnore = [sender stringValue];
  if (wordToIgnore != nil)
    spellcheck_platform::IgnoreWord(base::SysNSStringToUTF16(wordToIgnore));
}

- (void)showGuessPanel:(id)sender {
  const bool visible = spellcheck_platform::SpellingPanelVisible();

  content::WebContents* webContents = content::WebContents::FromRenderViewHost(
      RenderViewHost::From(renderWidgetHost_));
  DCHECK(webContents && webContents->GetFocusedFrame());

  mojo::Remote<spellcheck::mojom::SpellCheckPanel>
      focused_spell_check_panel_client;
  webContents->GetFocusedFrame()->GetRemoteInterfaces()->GetInterface(
      focused_spell_check_panel_client.BindNewPipeAndPassReceiver());
  focused_spell_check_panel_client->ToggleSpellPanel(visible);
}

- (void)toggleContinuousSpellChecking:(id)sender {
  content::RenderProcessHost* host = renderWidgetHost_->GetProcess();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  DCHECK(profile);
  PrefService* pref = profile->GetPrefs();
  pref->SetBoolean(spellcheck::prefs::kSpellCheckEnable,
                   !pref->GetBoolean(spellcheck::prefs::kSpellCheckEnable));
}

// END Spellchecking methods

// If a dialog is visible, make its window key. See becomeFirstResponder.
- (void)makeAnyDialogKey {
  if (const auto* contents = content::WebContents::FromRenderViewHost(
          RenderViewHost::From(renderWidgetHost_))) {
    if (const auto* manager =
            web_modal::WebContentsModalDialogManager::FromWebContents(
                contents)) {
      // IsDialogActive() returns true if a dialog exists.
      if (manager->IsDialogActive()) {
        manager->FocusTopmostDialog();
      }
    }
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
  NSWindow* browserWindow =
      [renderWidgetHost_->GetView()->GetNativeView().GetNativeNSView() window];
  DCHECK(browserWindow);

  // If the browser window is already key, there's nothing to do.
  if (browserWindow.isKeyWindow)
    return;

  // Otherwise, look for it in the key window's chain of parents.
  NSWindow* keyWindowOrParent = NSApp.keyWindow;
  while (keyWindowOrParent && keyWindowOrParent != browserWindow)
    keyWindowOrParent = keyWindowOrParent.parentWindow;

  // If the browser window isn't among the parents, there's nothing to do.
  if (keyWindowOrParent != browserWindow)
    return;

  // Otherwise, temporarily set an ivar so that -windowDidBecomeKey, below,
  // doesn't immediately make the dialog key.
  base::AutoReset<BOOL> scoped(&resigningFirstResponder_, YES);

  // …then make the browser window key.
  [browserWindow makeKeyWindow];
}

// If the browser window becomes key while the RenderWidgetHostView is first
// responder, make the dialog key (if there is one).
- (void)windowDidBecomeKey {
  if (resigningFirstResponder_)
    return;
  NSView* view =
      renderWidgetHost_->GetView()->GetNativeView().GetNativeNSView();
  if (view.window.firstResponder == view)
    [self makeAnyDialogKey];
}

@end
