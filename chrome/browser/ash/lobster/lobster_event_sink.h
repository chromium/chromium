// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_EVENT_SINK_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_EVENT_SINK_H_

class LobsterEventSink {
 public:
  virtual ~LobsterEventSink() = default;
  virtual void OnFocus(int context_id) = 0;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_EVENT_SINK_H_
