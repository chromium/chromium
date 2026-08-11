// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_AIM_MESSAGE_POSTER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_AIM_MESSAGE_POSTER_H_

namespace lens {
class ClientToAimMessage;
}  // namespace lens

namespace contextual_tasks {

class AimMessagePoster {
 public:
  virtual ~AimMessagePoster() = default;
  virtual void PostAimMessage(const lens::ClientToAimMessage& message) = 0;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_AIM_MESSAGE_POSTER_H_
