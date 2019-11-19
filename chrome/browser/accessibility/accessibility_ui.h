// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_UI_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_UI_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace content {
struct AXEventNotificationDetails;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Controls the accessibility web UI page.
class AccessibilityUI : public content::WebUIController {
 public:
  explicit AccessibilityUI(content::WebUI* web_ui);
  ~AccessibilityUI() override;
};

// Observes accessibility events from web contents.
class AccessibilityUIObserver : public content::WebContentsObserver {
 public:
  AccessibilityUIObserver(content::WebContents* web_contents,
                          std::vector<std::string>* event_logs);
  ~AccessibilityUIObserver() override;

  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& details) override;

 private:
  std::vector<std::string>* event_logs_;
};

// Manages messages sent from accessibility.js via json.
class AccessibilityUIMessageHandler : public content::WebUIMessageHandler {
 public:
  AccessibilityUIMessageHandler();
  ~AccessibilityUIMessageHandler() override;

  void RegisterMessages() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  std::vector<std::string> event_logs_;
  std::unique_ptr<AccessibilityUIObserver> observer_;

  void ToggleAccessibility(const base::ListValue* args);
  void SetGlobalFlag(const base::ListValue* args);
  void RequestWebContentsTree(const base::ListValue* args);
  void RequestNativeUITree(const base::ListValue* args);
  void RequestAccessibilityEvents(const base::ListValue* args);
  void Callback(const std::string&);

  DISALLOW_COPY_AND_ASSIGN(AccessibilityUIMessageHandler);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_UI_H_
