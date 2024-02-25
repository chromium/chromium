// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BAD_MESSAGE_H_
#define CHROME_BROWSER_BAD_MESSAGE_H_

namespace content {
class RenderProcessHost;
}

namespace bad_message {

// The browser process often chooses to terminate a renderer if it receives
// a bad IPC message. The reasons are tracked for metrics.
//
// See also content/browser/bad_message.h.
//
// NOTE: Do not remove or reorder elements in this list. Add new entries at the
// end. Items may be renamed but do not change the values. We rely on the enum
// values in histograms.
enum BadMessageReason {
  WRLHH_LOGGING_STOPPED_BAD_STATE = 0,
  PPH_EXTRA_PREVIEW_MESSAGE = 1,
  PMF_INVALID_INITIATOR_ORIGIN = 2,
  RFH_INVALID_WEB_UI_CONTROLLER = 3,
  RFH_DISPLAY_CAPTURE_PERMISSION_MISSING = 4,
  MSFD_MULTIPLE_CLOSURES_OF_FOCUSABILITY_WINDOW = 5,
  MSFD_MULTIPLE_EXPLICIT_CALLS_TO_FOCUS = 6,
  PVM_SCRIPTED_PRINT_FENCED_FRAME = 7,
  PVMB_SCRIPTED_PRINT_FENCED_FRAME = 8,
  SSI_CREATE_FENCED_FRAME = 9,
  CCU_SUPERFLUOUS_BIND = 10,

  // Please add new elements here. The naming convention is abbreviated class
  // name (e.g. RenderFrameHost becomes RFH) plus a unique description of the
  // reason. After making changes, you MUST update histograms.xml by running:
  // "python tools/metrics/histograms/update_bad_message_reasons.py"
  BAD_MESSAGE_MAX
};

// Called when the browser receives a bad IPC message from a renderer process on
// the UI thread. Logs the event, records a histogram metric for the |reason|,
// and terminates the process for |host|.
void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason);

}  // namespace bad_message

#endif  // CHROME_BROWSER_BAD_MESSAGE_H_
