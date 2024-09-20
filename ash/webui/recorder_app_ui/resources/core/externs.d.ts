// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set audio output device in AudioContext, available from Chrome 110
// https://webaudio.github.io/web-audio-api/#AudioContextOptions
type AudioSinkType = 'none';

interface AudioSinkOptions {
  type: AudioSinkType;
}

interface AudioContextOptions {
  sinkId?: AudioSinkOptions|string;
}

/*
 * This is the return value for LitElement render function.
 *
 * Since the render function can return multiple different renderable types [1],
 * the type gets really complex if we explicitly list all possible types.
 * LitElement own typing use `unknown` for render return type, and upstream
 * discussion [2] also suggests using `unknown`, so we just alias the type to
 * `unknown` and don't further restrict what types can be returned by render.
 *
 * Since directly writing `unknown` as return type of the render function is
 * a bit confusing to readers, we expose a type alias here makes the code more
 * readable.
 *
 * Also see
 * https://chromium-review.googlesource.com/c/chromium/src/+/4318288/comment/c7a4600e_6ce078bc/
 *
 * [1]: https://lit.dev/docs/components/rendering/#renderable-values
 * [2]: https://github.com/lit/lit/discussions/2359
 */

type RenderResult = unknown;

// Chrome private API for crash report.
declare namespace chrome.crashReportPrivate {
  export interface ErrorInfo {
    message: string;
    url: string;
    columnNumber?: number;
    debugId?: string;
    lineNumber?: number;
    product?: string;
    stackTrace?: string;
    version?: string;
  }
  export const reportError: (info: ErrorInfo, callback: () => void) => void;
}

/*
 * View Transition API.
 *
 * See https://developer.chrome.com/docs/web-platform/view-transitions or
 * https://developer.mozilla.org/en-US/docs/Web/API/View_Transitions_API for
 * detail.
 */
interface ViewTransition {
  updateCallbackDone: Promise<void>;
  ready: Promise<void>;
  finished: Promise<void>;
  skipTransition(): void;
}

interface Document {
  startViewTransition?(
    updateCallback: () => Promise<void>| void,
  ): ViewTransition;
}
