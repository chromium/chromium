// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_

#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/extension_id.h"
#include "ui/accessibility/ax_tree_id.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
struct AXActionData;
}  // namespace ui

struct ExtensionMsg_AccessibilityEventBundleParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace extensions {
struct AutomationListener;

class AutomationEventRouter : public content::NotificationObserver {
 public:
  static AutomationEventRouter* GetInstance();

  // Indicates that the listener at |listener_process_id| wants to receive
  // automation events from the accessibility tree indicated by
  // |source_ax_tree_id|. Automation events are forwarded from now on until the
  // listener process dies.
  void RegisterListenerForOneTree(const ExtensionId& extension_id,
                                  int listener_process_id,
                                  ui::AXTreeID source_ax_tree_id);

  // Indicates that the listener at |listener_process_id| wants to receive
  // automation events from all accessibility trees because it has Desktop
  // permission.
  void RegisterListenerWithDesktopPermission(const ExtensionId& extension_id,
                                             int listener_process_id);

  void DispatchAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events);

  void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params);

  // Notify all automation extensions that an accessibility tree was
  // destroyed. If |browser_context| is null,
  void DispatchTreeDestroyedEvent(ui::AXTreeID tree_id,
                                  content::BrowserContext* browser_context);

  // Notify the source extension of the action of an action result.
  void DispatchActionResult(const ui::AXActionData& data, bool result);

  void SetTreeDestroyedCallbackForTest(
      base::RepeatingCallback<void(ui::AXTreeID)> cb);

 private:
  struct AutomationListener {
    AutomationListener();
    AutomationListener(const AutomationListener& other);
    ~AutomationListener();

    ExtensionId extension_id;
    int process_id;
    bool desktop;
    std::set<ui::AXTreeID> tree_ids;
    bool is_active_profile;
  };

  AutomationEventRouter();
  ~AutomationEventRouter() override;

  void Register(const ExtensionId& extension_id,
                int listener_process_id,
                ui::AXTreeID source_ax_tree_id,
                bool desktop);

  // content::NotificationObserver interface.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called when the user switches profiles or when a listener is added
  // or removed. The purpose is to ensure that multiple instances of the
  // same extension running in different profiles don't interfere with one
  // another, so in that case only the one associated with the active profile
  // is marked as active.
  //
  // This is needed on Chrome OS because ChromeVox loads into the login profile
  // in addition to the active profile.  If a similar fix is needed on other
  // platforms, we'd need an equivalent of SessionStateObserver that works
  // everywhere.
  void UpdateActiveProfile();

  content::NotificationRegistrar registrar_;
  std::vector<AutomationListener> listeners_;

  Profile* active_profile_;

  base::RepeatingCallback<void(ui::AXTreeID)> tree_destroyed_callback_for_test_;

  friend struct base::DefaultSingletonTraits<AutomationEventRouter>;

  DISALLOW_COPY_AND_ASSIGN(AutomationEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_
