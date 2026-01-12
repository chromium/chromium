// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/action_runner/action_runner.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/mcp_server/accessibility_snapshot/accessibility_snapshot.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace mcp_server {

namespace {

// Poll interval for WaitForSelector (100ms)
constexpr base::TimeDelta kPollInterval = base::Milliseconds(100);

// JavaScript helper: Escape string for use in JS code
std::string EscapeJsString(const std::string& str) {
  std::string escaped;
  base::EscapeJSONString(str, false, &escaped);  // Don't add quotes
  return "\"" + escaped + "\"";  // Add quotes manually for better control
}

}  // namespace

ActionRunner::ActionRunner() = default;
ActionRunner::~ActionRunner() = default;

void ActionRunner::Click(content::WebContents* web_contents,
                         const std::string& selector,
                         ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string script = ClickElementScript(selector);
  ExecuteScript(web_contents, script, std::move(callback));
}

void ActionRunner::Type(content::WebContents* web_contents,
                        const std::string& selector,
                        const std::string& text,
                        ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string script = TypeIntoElementScript(selector, text);
  ExecuteScript(web_contents, script, std::move(callback));
}

void ActionRunner::Hover(content::WebContents* web_contents,
                         const std::string& selector,
                         ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string script = HoverElementScript(selector);
  ExecuteScript(web_contents, script, std::move(callback));
}

void ActionRunner::SelectOption(content::WebContents* web_contents,
                                const std::string& selector,
                                const std::string& value,
                                ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string script = SelectOptionScript(selector, value);
  ExecuteScript(web_contents, script, std::move(callback));
}

void ActionRunner::WaitForSelector(content::WebContents* web_contents,
                                   const std::string& selector,
                                   int timeout_ms,
                                   ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Start polling
  PollForSelector(web_contents, selector, timeout_ms, 0, std::move(callback));
}

void ActionRunner::Evaluate(content::WebContents* web_contents,
                            const std::string& js_code,
                            ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ExecuteScript(web_contents, js_code, std::move(callback));
}

void ActionRunner::Screenshot(content::WebContents* web_contents,
                              bool full_page,
                              ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Use CDP for screenshot
  CaptureScreenshotCDP(web_contents, full_page, std::move(callback));
}

void ActionRunner::ExecuteScript(content::WebContents* web_contents,
                                 const std::string& script,
                                 ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    std::move(callback).Run(false, "WebContents is null", base::Value::Dict());
    return;
  }

  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  if (!frame) {
    std::move(callback).Run(false, "No main frame", base::Value::Dict());
    return;
  }

  // Execute JavaScript and get result
  frame->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(std::string_view(script)),
      base::BindOnce(&ActionRunner::OnScriptExecuted,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback)),
      content::ISOLATED_WORLD_ID_GLOBAL);
}

void ActionRunner::OnScriptExecuted(ActionCallback callback,
                                    base::Value result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if result indicates error
  if (result.is_dict()) {
    const base::Value::Dict& dict = result.GetDict();
    const std::string* error = dict.FindString("error");
    if (error) {
      std::move(callback).Run(false, *error, base::Value::Dict());
      return;
    }

    // Success with data
    const base::Value::Dict* data = dict.FindDict("data");
    if (data) {
      std::move(callback).Run(true, "", data->Clone());
      return;
    }
  }

  // Success with result as data
  base::Value::Dict data;
  if (result.is_string()) {
    data.Set("result", result.GetString());
  } else if (result.is_int()) {
    data.Set("result", result.GetInt());
  } else if (result.is_bool()) {
    data.Set("result", result.GetBool());
  } else if (result.is_double()) {
    data.Set("result", result.GetDouble());
  } else if (result.is_dict()) {
    data = result.GetDict().Clone();
  }

  std::move(callback).Run(true, "", std::move(data));
}

std::string ActionRunner::ClickElementScript(const std::string& selector) {
  return base::StringPrintf(R"js(
(function() {
  try {
    const element = document.querySelector(%s);
    if (!element) {
      return {error: 'Element not found: %s'};
    }

    const rect = element.getBoundingClientRect();

    // Scroll element into view if needed
    element.scrollIntoView({behavior: 'instant', block: 'center'});

    // Dispatch mouse events
    const mousedownEvent = new MouseEvent('mousedown', {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: rect.left + rect.width / 2,
      clientY: rect.top + rect.height / 2
    });
    element.dispatchEvent(mousedownEvent);

    const clickEvent = new MouseEvent('click', {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: rect.left + rect.width / 2,
      clientY: rect.top + rect.height / 2
    });
    element.dispatchEvent(clickEvent);

    const mouseupEvent = new MouseEvent('mouseup', {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: rect.left + rect.width / 2,
      clientY: rect.top + rect.height / 2
    });
    element.dispatchEvent(mouseupEvent);

    // Also trigger focus and native click
    element.focus();
    element.click();

    return {
      data: {
        success: true,
        bounds: {
          x: rect.x,
          y: rect.y,
          width: rect.width,
          height: rect.height
        }
      }
    };
  } catch (e) {
    return {error: 'Click failed: ' + e.message};
  }
})();
)js",
      EscapeJsString(selector).c_str(),
      EscapeJsString(selector).c_str());
}

std::string ActionRunner::TypeIntoElementScript(const std::string& selector,
                                                const std::string& text) {
  return base::StringPrintf(R"js(
(function() {
  try {
    const element = document.querySelector(%s);
    if (!element) {
      return {error: 'Element not found: %s'};
    }

    // Check if element is editable
    const tagName = element.tagName.toLowerCase();
    const isInput = tagName === 'input' || tagName === 'textarea';
    const isContentEditable = element.isContentEditable;

    if (!isInput && !isContentEditable) {
      return {error: 'Element is not editable: ' + tagName};
    }

    // Focus the element
    element.focus();
    element.scrollIntoView({behavior: 'instant', block: 'center'});

    // Clear existing text
    if (isInput) {
      element.value = '';
    } else {
      element.textContent = '';
    }

    // Dispatch input events
    element.dispatchEvent(new Event('focus', {bubbles: true}));

    const text = %s;

    // Set the text
    if (isInput) {
      element.value = text;
    } else {
      element.textContent = text;
    }

    // Dispatch input/change events
    element.dispatchEvent(new Event('input', {bubbles: true}));
    element.dispatchEvent(new Event('change', {bubbles: true}));

    return {
      data: {
        success: true,
        text: text,
        element: tagName
      }
    };
  } catch (e) {
    return {error: 'Type failed: ' + e.message};
  }
})();
)js",
      EscapeJsString(selector).c_str(),
      EscapeJsString(selector).c_str(),
      EscapeJsString(text).c_str());
}

std::string ActionRunner::HoverElementScript(const std::string& selector) {
  return base::StringPrintf(R"js(
(function() {
  try {
    const element = document.querySelector(%s);
    if (!element) {
      return {error: 'Element not found: %s'};
    }

    const rect = element.getBoundingClientRect();

    // Scroll element into view
    element.scrollIntoView({behavior: 'instant', block: 'center'});

    // Dispatch mouse events for hover
    const mouseenterEvent = new MouseEvent('mouseenter', {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: rect.left + rect.width / 2,
      clientY: rect.top + rect.height / 2
    });
    element.dispatchEvent(mouseenterEvent);

    const mouseoverEvent = new MouseEvent('mouseover', {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: rect.left + rect.width / 2,
      clientY: rect.top + rect.height / 2
    });
    element.dispatchEvent(mouseoverEvent);

    const mousemoveEvent = new MouseEvent('mousemove', {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: rect.left + rect.width / 2,
      clientY: rect.top + rect.height / 2
    });
    element.dispatchEvent(mousemoveEvent);

    return {
      data: {
        success: true,
        bounds: {
          x: rect.x,
          y: rect.y,
          width: rect.width,
          height: rect.height
        }
      }
    };
  } catch (e) {
    return {error: 'Hover failed: ' + e.message};
  }
})();
)js",
      EscapeJsString(selector).c_str(),
      EscapeJsString(selector).c_str());
}

std::string ActionRunner::SelectOptionScript(const std::string& selector,
                                             const std::string& value) {
  return base::StringPrintf(R"js(
(function() {
  try {
    const element = document.querySelector(%s);
    if (!element) {
      return {error: 'Element not found: %s'};
    }

    if (element.tagName.toLowerCase() !== 'select') {
      return {error: 'Element is not a select: ' + element.tagName};
    }

    const targetValue = %s;

    // Try to find option by value first
    let option = Array.from(element.options).find(opt => opt.value === targetValue);

    // If not found, try by text content
    if (!option) {
      option = Array.from(element.options).find(opt => opt.textContent.trim() === targetValue);
    }

    if (!option) {
      return {error: 'Option not found: ' + targetValue};
    }

    // Select the option
    element.value = option.value;
    option.selected = true;

    // Dispatch change event
    element.dispatchEvent(new Event('change', {bubbles: true}));
    element.dispatchEvent(new Event('input', {bubbles: true}));

    return {
      data: {
        success: true,
        selected: option.value,
        text: option.textContent.trim()
      }
    };
  } catch (e) {
    return {error: 'SelectOption failed: ' + e.message};
  }
})();
)js",
      EscapeJsString(selector).c_str(),
      EscapeJsString(selector).c_str(),
      EscapeJsString(value).c_str());
}

std::string ActionRunner::CheckElementExistsScript(
    const std::string& selector) {
  return base::StringPrintf(R"js(
(function() {
  const element = document.querySelector(%s);
  return {
    data: {
      exists: !!element,
      visible: element ? (element.offsetParent !== null) : false
    }
  };
})();
)js",
      EscapeJsString(selector).c_str());
}

std::string ActionRunner::GetElementCenterScript(const std::string& selector) {
  return base::StringPrintf(R"js(
(function() {
  try {
    const element = document.querySelector(%s);
    if (!element) {
      return {error: 'Element not found: %s'};
    }

    const rect = element.getBoundingClientRect();
    return {
      data: {
        x: rect.x + rect.width / 2,
        y: rect.y + rect.height / 2,
        bounds: {
          x: rect.x,
          y: rect.y,
          width: rect.width,
          height: rect.height
        }
      }
    };
  } catch (e) {
    return {error: 'GetElementCenter failed: ' + e.message};
  }
})();
)js",
      EscapeJsString(selector).c_str(),
      EscapeJsString(selector).c_str());
}

void ActionRunner::PollForSelector(content::WebContents* web_contents,
                                   const std::string& selector,
                                   int timeout_ms,
                                   int elapsed_ms,
                                   ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    std::move(callback).Run(false, "WebContents is null", base::Value::Dict());
    return;
  }

  // Check timeout
  if (elapsed_ms >= timeout_ms) {
    std::move(callback).Run(false,
                           base::StringPrintf("Timeout waiting for selector: %s",
                                            selector.c_str()),
                           base::Value::Dict());
    return;
  }

  // Check if element exists
  std::string script = CheckElementExistsScript(selector);

  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  if (!frame) {
    std::move(callback).Run(false, "No main frame", base::Value::Dict());
    return;
  }

  frame->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(std::string_view(script)),
      base::BindOnce(
          [](base::WeakPtr<ActionRunner> self,
             content::WebContents* web_contents,
             const std::string& selector,
             int timeout_ms,
             int elapsed_ms,
             ActionCallback callback,
             base::Value result) {
            if (!self) {
              std::move(callback).Run(false, "ActionRunner destroyed",
                                     base::Value::Dict());
              return;
            }

            // Check if element exists and is visible
            bool exists = false;
            bool visible = false;

            if (result.is_dict()) {
              const base::Value::Dict& dict = result.GetDict();
              const base::Value::Dict* data = dict.FindDict("data");
              if (data) {
                exists = data->FindBool("exists").value_or(false);
                visible = data->FindBool("visible").value_or(false);
              }
            }

            if (exists && visible) {
              // Element found!
              base::Value::Dict result_data;
              result_data.Set("selector", selector);
              result_data.Set("elapsed_ms", elapsed_ms);
              std::move(callback).Run(true, "", std::move(result_data));
              return;
            }

            // Not found yet, schedule next poll
            int new_elapsed = elapsed_ms + kPollInterval.InMilliseconds();

            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(&ActionRunner::PollForSelector,
                              self,
                              web_contents,
                              selector,
                              timeout_ms,
                              new_elapsed,
                              std::move(callback)),
                kPollInterval);
          },
          weak_factory_.GetWeakPtr(),
          web_contents,
          selector,
          timeout_ms,
          elapsed_ms,
          std::move(callback)),
      content::ISOLATED_WORLD_ID_GLOBAL);
}

void ActionRunner::CaptureScreenshotCDP(content::WebContents* web_contents,
                                        bool full_page,
                                        ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO: Implement via Chrome DevTools Protocol
  // For now, return error
  std::move(callback).Run(
      false,
      "Screenshot not yet implemented - requires CDP integration",
      base::Value::Dict());
}

// ===== Reference ID-based action implementations =====

void ActionRunner::ClickByRef(content::WebContents* web_contents,
                               const std::string& ref_id,
                               ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Convert ref ID to CSS selector
  std::string selector = AccessibilitySnapshot::GetSelectorForRef(web_contents, ref_id);

  if (selector.empty()) {
    std::move(callback).Run(
        false,
        "Reference ID not found: " + ref_id + ". Take a fresh accessibility snapshot first.",
        base::Value::Dict());
    return;
  }

  // Call the selector-based method
  Click(web_contents, selector, std::move(callback));
}

void ActionRunner::TypeByRef(content::WebContents* web_contents,
                              const std::string& ref_id,
                              const std::string& text,
                              ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Convert ref ID to CSS selector
  std::string selector = AccessibilitySnapshot::GetSelectorForRef(web_contents, ref_id);

  if (selector.empty()) {
    std::move(callback).Run(
        false,
        "Reference ID not found: " + ref_id + ". Take a fresh accessibility snapshot first.",
        base::Value::Dict());
    return;
  }

  // Call the selector-based method
  Type(web_contents, selector, text, std::move(callback));
}

void ActionRunner::HoverByRef(content::WebContents* web_contents,
                               const std::string& ref_id,
                               ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Convert ref ID to CSS selector
  std::string selector = AccessibilitySnapshot::GetSelectorForRef(web_contents, ref_id);

  if (selector.empty()) {
    std::move(callback).Run(
        false,
        "Reference ID not found: " + ref_id + ". Take a fresh accessibility snapshot first.",
        base::Value::Dict());
    return;
  }

  // Call the selector-based method
  Hover(web_contents, selector, std::move(callback));
}

void ActionRunner::SelectOptionByRef(content::WebContents* web_contents,
                                      const std::string& ref_id,
                                      const std::string& value,
                                      ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Convert ref ID to CSS selector
  std::string selector = AccessibilitySnapshot::GetSelectorForRef(web_contents, ref_id);

  if (selector.empty()) {
    std::move(callback).Run(
        false,
        "Reference ID not found: " + ref_id + ". Take a fresh accessibility snapshot first.",
        base::Value::Dict());
    return;
  }

  // Call the selector-based method
  SelectOption(web_contents, selector, value, std::move(callback));
}

// ===== Scroll action implementation =====

void ActionRunner::Scroll(content::WebContents* web_contents,
                          const std::string& mode,
                          int x,
                          int y,
                          const std::string& selector,
                          const std::string& behavior,
                          ActionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string script;

  if (mode == "scrollBy") {
    // Scroll by relative amount
    script = base::StringPrintf(
        "(function() {"
        "  window.scrollBy({left: %d, top: %d, behavior: '%s'});"
        "  return {x: window.scrollX, y: window.scrollY};"
        "})()",
        x, y, behavior.c_str());
  } else if (mode == "scrollTo") {
    // Scroll to absolute position
    script = base::StringPrintf(
        "(function() {"
        "  window.scrollTo({left: %d, top: %d, behavior: '%s'});"
        "  return {x: window.scrollX, y: window.scrollY};"
        "})()",
        x, y, behavior.c_str());
  } else if (mode == "scrollIntoView") {
    // Scroll element into view
    if (selector.empty()) {
      std::move(callback).Run(
          false,
          "scrollIntoView mode requires a selector",
          base::Value::Dict());
      return;
    }

    std::string escaped_selector = EscapeJsString(selector);
    script = base::StringPrintf(
        "(function() {"
        "  const element = document.querySelector(%s);"
        "  if (!element) {"
        "    throw new Error('Element not found: %s');"
        "  }"
        "  element.scrollIntoView({behavior: '%s', block: 'center', inline: 'nearest'});"
        "  return {x: window.scrollX, y: window.scrollY};"
        "})()",
        escaped_selector.c_str(), selector.c_str(), behavior.c_str());
  } else {
    std::move(callback).Run(
        false,
        "Invalid scroll mode. Must be: scrollBy, scrollTo, or scrollIntoView",
        base::Value::Dict());
    return;
  }

  ExecuteScript(web_contents, script, std::move(callback));
}

}  // namespace mcp_server
