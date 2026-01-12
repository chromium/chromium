// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_ACTION_RUNNER_ACTION_RUNNER_H_
#define CHROME_BROWSER_MCP_SERVER_ACTION_RUNNER_ACTION_RUNNER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace content {
class WebContents;
}

namespace mcp_server {

// ActionRunner executes UI interactions on web pages
// Uses JavaScript evaluation for element-based actions
// Uses CDP Input domain for low-level mouse/keyboard events
class ActionRunner {
 public:
  ActionRunner();
  ~ActionRunner();

  // Result callback: (success, error_message, result_data)
  using ActionCallback =
      base::OnceCallback<void(bool, const std::string&, base::Value::Dict)>;

  // Click on element matching selector
  // Returns element bounds if successful
  void Click(content::WebContents* web_contents,
             const std::string& selector,
             ActionCallback callback);

  // Type text into element matching selector
  // Clears existing text first, then types with realistic delays
  void Type(content::WebContents* web_contents,
            const std::string& selector,
            const std::string& text,
            ActionCallback callback);

  // Hover over element matching selector
  // Useful for dropdowns and tooltips
  void Hover(content::WebContents* web_contents,
             const std::string& selector,
             ActionCallback callback);

  // Select option in dropdown by value or text
  // Works with <select> elements
  void SelectOption(content::WebContents* web_contents,
                    const std::string& selector,
                    const std::string& value,
                    ActionCallback callback);

  // Wait for element matching selector to appear
  // Polls with timeout (default 30 seconds)
  void WaitForSelector(content::WebContents* web_contents,
                       const std::string& selector,
                       int timeout_ms,
                       ActionCallback callback);

  // Execute arbitrary JavaScript code
  // Returns the result as JSON-serializable value
  void Evaluate(content::WebContents* web_contents,
                const std::string& js_code,
                ActionCallback callback);

  // Capture screenshot of the page
  // Returns base64-encoded PNG image
  void Screenshot(content::WebContents* web_contents,
                  bool full_page,
                  ActionCallback callback);

  // ===== Reference ID-based actions =====
  // These methods work with ref IDs from accessibility snapshots
  // They convert ref IDs to CSS selectors and call the selector-based methods

  // Click on element by reference ID
  void ClickByRef(content::WebContents* web_contents,
                  const std::string& ref_id,
                  ActionCallback callback);

  // Type text into element by reference ID
  void TypeByRef(content::WebContents* web_contents,
                 const std::string& ref_id,
                 const std::string& text,
                 ActionCallback callback);

  // Hover over element by reference ID
  void HoverByRef(content::WebContents* web_contents,
                  const std::string& ref_id,
                  ActionCallback callback);

  // Select option by reference ID
  void SelectOptionByRef(content::WebContents* web_contents,
                         const std::string& ref_id,
                         const std::string& value,
                         ActionCallback callback);

  // Scroll the page or element into view
  // Supports multiple modes: scrollBy, scrollTo, scrollIntoView
  // Returns new scroll position after scrolling
  void Scroll(content::WebContents* web_contents,
              const std::string& mode,  // "scrollBy", "scrollTo", or "scrollIntoView"
              int x,
              int y,
              const std::string& selector,  // For scrollIntoView mode
              const std::string& behavior,  // "smooth" or "auto"
              ActionCallback callback);

 private:
  // Execute JavaScript and get result
  void ExecuteScript(content::WebContents* web_contents,
                     const std::string& script,
                     ActionCallback callback);

  // Callback for JavaScript execution
  void OnScriptExecuted(ActionCallback callback, base::Value result);

  // Helper: Get element center coordinates
  std::string GetElementCenterScript(const std::string& selector);

  // Helper: Check if element exists
  std::string CheckElementExistsScript(const std::string& selector);

  // Helper: Simulate click via JS
  std::string ClickElementScript(const std::string& selector);

  // Helper: Type into element
  std::string TypeIntoElementScript(const std::string& selector,
                                     const std::string& text);

  // Helper: Hover over element
  std::string HoverElementScript(const std::string& selector);

  // Helper: Select option
  std::string SelectOptionScript(const std::string& selector,
                                  const std::string& value);

  // Helper: Capture screenshot via CDP
  void CaptureScreenshotCDP(content::WebContents* web_contents,
                             bool full_page,
                             ActionCallback callback);

  // Wait for selector implementation (polling)
  void PollForSelector(content::WebContents* web_contents,
                       const std::string& selector,
                       int timeout_ms,
                       int elapsed_ms,
                       ActionCallback callback);

  base::WeakPtrFactory<ActionRunner> weak_factory_{this};
};

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_ACTION_RUNNER_ACTION_RUNNER_H_
