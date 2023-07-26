// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_EVENT_SINK_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_EVENT_SINK_H_

namespace ash {
namespace input_method {

class EditorEventSink {
 public:
  virtual ~EditorEventSink() = default;
  virtual void OnFocus(int context_id) = 0;
  virtual void OnBlur() = 0;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_EVENT_SINK_H_
