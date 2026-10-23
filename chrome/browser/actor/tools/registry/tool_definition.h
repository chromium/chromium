// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_H_
#define CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_H_

#include <string>

#include "base/values.h"

namespace actor {

// Identifier for each shared browser tool in ActorKeyedService.
//
// These values may be persisted to logs. Entries are append-only: do not
// renumber or reuse numeric values, and update `kMaxValue` when adding new
// entries.
enum class ToolId {
  // Navigates the active tab to a specified URL.
  kNavigate = 0,
  // Clicks an element on the active page by DOM node ID.
  kClick = 1,
  // Switches to an open tab in the active browser window matching a query.
  kSwitchTab = 2,
  // Scrolls an element or the main viewport in the active tab.
  kScroll = 3,
  // Selects an option in a dropdown (<select>) element on the page.
  kSelectOption = 4,
  // Types text into an editable element on the page.
  kType = 5,
  // Translates the current page.
  kTranslatePage = 6,
  // Navigates the active tab backward one entry in session history.
  kGoBack = 7,
  // Navigates the active tab forward one entry in session history.
  kGoForward = 8,
  // Reloads the current page in the active tab.
  kReloadPage = 9,
  // Seeks to a specific timestamp in the media in the active tab.
  kSeekToTimestamp = 10,
  // Pauses media playback in the active tab.
  kPauseVideo = 11,
  // Starts or resumes media playback in the active tab.
  kPlayVideo = 12,
  // Search using the default search engine.
  kPerformSearch = 13,
  // Exits fullscreen mode for the active tab.
  kExitFullscreen = 14,
  // Opens a known URL or browser page.
  kOpenKnownPage = 15,
  // Fills a one-time password (OTP) into input fields on the page.
  kAttemptOtpFilling = 16,
  // Enters fullscreen mode for a window.
  kEnterFullscreen = 17,
  // Highlights matching text in the active tab and scrolls it into view.
  kFindAndHighlight = 18,
  // Closes the active tab.
  kCloseTab = 19,
  // Uploads files through a file input or file chooser on the page.
  kFileUpload = 20,
  // Creates a new tab.
  kCreateTab = 21,
  // Activates an open tab by its ID.
  kActivateTab = 22,
  // Scrolls an element into view in the active tab.
  kScrollTo = 23,
  // Adds a bookmark for the specified URL and title.
  kAddBookmark = 24,
  // Removes bookmarks matching the specified URL.
  kRemoveBookmark = 25,
  // Scrolls an element or the main viewport by a page or to its start/end.
  kScrollPage = 26,
  // Opens a new browser window.
  kCreateWindow = 27,
  // Brings the specified window to the foreground.
  kActivateWindow = 28,
  // Closes the specified window.
  kCloseWindow = 29,
  // Moves the mouse pointer over an element on the page.
  kMoveMouse = 30,
  // Drags an element and drops it onto another element on the page.
  kDragAndRelease = 31,
  // Pauses execution for a specified duration in milliseconds.
  kWait = 32,
  // Executes a script tool in a document.
  kScript = 33,
  // Loads a set of URLs in background tabs and extracts their page content.
  kLoadAndExtractContent = 34,
  // Attempts to fill forms on the active webpage with user information.
  kAttemptFormFilling = 35,
  // Attempts to log in to the website using saved credentials or social login
  // buttons.
  kAttemptLogin = 36,
  // Opens the Gemini side panel with a prompt.
  kPromptGeminiPanel = 37,
  // Closes the Gemini side panel if it is currently open.
  kCloseGeminiPanel = 38,
  // Opens a URL in the given tab or in a new tab in that tab's window.
  kOpenUrl = 39,
  kMaxValue = kOpenUrl,
};

// In-memory metadata and parameter schema for a shared browser tool. Prefer
// building instances with `ToolDefinitionBuilder`.
struct ToolDefinition {
  ToolDefinition(ToolId id, std::string name, std::string description);
  ToolDefinition(ToolId id,
                 std::string name,
                 std::string description,
                 base::DictValue parameters_json_schema);
  ~ToolDefinition();

  ToolDefinition(const ToolDefinition&) = delete;
  ToolDefinition& operator=(const ToolDefinition&) = delete;
  ToolDefinition(ToolDefinition&&);
  ToolDefinition& operator=(ToolDefinition&&);

  // Needed to make a copy of `ToolDefinition`, because `base::DictValue` is
  // move-only.
  ToolDefinition Clone() const;

  // Chrome-internal identifier for this tool.
  ToolId id;

  // Model-facing name of this tool (e.g. "navigate").
  std::string name;

  // Client-side description of what this tool does and when to invoke it.
  std::string description;

  // JSON Schema object dictionary describing the tool's input parameters.
  base::DictValue parameters_json_schema;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_H_
