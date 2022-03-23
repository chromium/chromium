// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is currently a Chrome only API, and the spec is still in working draft
// stage.
// https://developer.mozilla.org/en-US/docs/Web/API/UIEvent/sourceCapabilities

interface UIEvent extends Event {
  readonly sourceCapabilities: InputDeviceCapabilities|null;
}

declare class InputDeviceCapabilities {
  constructor(param: {firesTouchEvents: boolean});
  readonly firesTouchEvents: boolean;
}

interface UIEventInit {
  sourceCapabilities?: InputDeviceCapabilities|null;
}
