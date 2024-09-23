// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SPARKY_EVENT_REWRITER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SPARKY_EVENT_REWRITER_H_

#include "ui/events/event_rewriter.h"

namespace app_list {

class SparkyEventRewriter : public ui::EventRewriter {
 public:
  SparkyEventRewriter() = default;

  SparkyEventRewriter(const SparkyEventRewriter&) = delete;
  SparkyEventRewriter& operator=(const SparkyEventRewriter&) = delete;

  ~SparkyEventRewriter() override = default;

  bool is_shift_pressed() { return shift_pressed_; }

  // ui::EventRewriter
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  bool shift_pressed_ = false;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SPARKY_EVENT_REWRITER_H_
