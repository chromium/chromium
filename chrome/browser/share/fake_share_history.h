// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_FAKE_SHARE_HISTORY_H_
#define CHROME_BROWSER_SHARE_FAKE_SHARE_HISTORY_H_

#include "chrome/browser/share/share_history.h"

namespace sharing {

// A test fake for ShareHistory, which allows directly injecting the history to
// be returned from GetFlatShareHistory().
class FakeShareHistory : public ShareHistory {
 public:
  FakeShareHistory();
  ~FakeShareHistory() override;

  void set_history(std::vector<Target> history) { history_ = history; }

  void AddShareEntry(const std::string& component_name) override;
  void GetFlatShareHistory(GetFlatHistoryCallback callback,
                           int window = -1) override;

 private:
  std::vector<Target> history_;
};

}  // namespace sharing

#endif  // CHROME_BROWSER_SHARE_FAKE_SHARE_HISTORY_H_
