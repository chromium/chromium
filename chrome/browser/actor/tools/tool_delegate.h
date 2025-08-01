// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_

namespace actor_login {
class ActorLoginService;
}  // namespace actor_login

namespace actor {

class AggregatedJournal;

// Provides tools with functionality implemented by the code invoking the tool.
class ToolDelegate {
 public:
  virtual ~ToolDelegate() = default;

  // Returns the journal so that tools may log information related to their
  // execution.
  virtual AggregatedJournal& GetJournal() = 0;

  // Returns the login service associated with the task.
  virtual actor_login::ActorLoginService& GetActorLoginService() = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_DELEGATE_H_
