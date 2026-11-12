// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SHAKE_TRIGGER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SHAKE_TRIGGER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace blink {
class WebInputEvent;
class WebMouseEvent;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace glic {

class WiggleDetector;

class ShakeTriggerClient {
 public:
  virtual ~ShakeTriggerClient() = default;
  // Returns true if the text selection is shared for the current profile.
  virtual bool IsTextSelectionSharingEnabled() const = 0;
  // Returns true if the Glic side panel is open.
  virtual bool IsSidePanelOpen() const = 0;
};

class ShakeTrigger {
 public:
  ShakeTrigger(content::WebContents* web_contents, ShakeTriggerClient& client);
  ShakeTrigger(const ShakeTrigger&) = delete;
  ShakeTrigger& operator=(const ShakeTrigger&) = delete;
  virtual ~ShakeTrigger();

  void OnInputEvent(const blink::WebInputEvent& event);

  // Returns true if mouse shake trigger is enabled by feature flag and pref.
  bool IsEnabled() const;

 protected:
  // Triggers Glic region capture when a mouse shake is detected.
  // Virtual for testing.
  virtual void TriggerRegionCapture();

 private:
  void ProcessMouseMove(const blink::WebMouseEvent& mouse_event);

  base::WeakPtr<content::WebContents> web_contents_;
  const raw_ref<ShakeTriggerClient> client_;

  const std::unique_ptr<WiggleDetector> detector_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SHAKE_TRIGGER_H_
