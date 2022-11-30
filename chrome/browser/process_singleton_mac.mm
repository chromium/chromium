// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>

#include "base/mac/scoped_aedesc.h"

namespace {

// Extracts the URL from |event| and forwards it to an already-running Chromium
// process.
OSErr HandleGURLEvent(const AppleEvent* event,
                      AppleEvent* reply,
                      SRefCon handler_refcon) {
  pid_t forwarding_pid = *(reinterpret_cast<pid_t*>(handler_refcon));
  base::mac::ScopedAEDesc<> other_process_pid;
  // Create an address descriptor for the running process.
  AECreateDesc(typeKernelProcessID, &forwarding_pid, sizeof(forwarding_pid),
               other_process_pid.OutPointer());

  OSErr status = noErr;
  base::mac::ScopedAEDesc<> event_copy;
  status = AECreateAppleEvent(kInternetEventClass, kAEGetURL, other_process_pid,
                              kAutoGenerateReturnID, kAnyTransactionID,
                              event_copy.OutPointer());
  if (status != noErr)
    return status;

  base::mac::ScopedAEDesc<> url;
  // A GURL event's direct object is the URL as a descriptor with type
  // TEXT.
  status =
      AEGetParamDesc(event, keyDirectObject, typeWildCard, url.OutPointer());
  if (status != noErr)
    return status;

  status = AEPutParamDesc(event_copy.OutPointer(), keyDirectObject, url);
  if (status != noErr)
    return status;

  status = AESendMessage(event_copy, reply, kAENoReply, kNoTimeOut);
  if (status != noErr)
    return status;

  // Activate the running instance
  base::mac::ScopedAEDesc<> activate_event;
  status = AECreateAppleEvent(kAEMiscStandards, kAEActivate, other_process_pid,
                              kAutoGenerateReturnID, kAnyTransactionID,
                              activate_event.OutPointer());
  if (status != noErr)
    return status;

  return AESendMessage(activate_event, reply, kAENoReply, kNoTimeOut);
}

}  //  namespace

bool ProcessSingleton::WaitForAndForwardOpenURLEvent(
    pid_t event_destination_pid) {
  AEEventHandlerProcPtr handler = NewAEEventHandlerUPP(HandleGURLEvent);
  if (AEInstallEventHandler(kInternetEventClass, kAEGetURL, handler,
                            &event_destination_pid, false) != noErr) {
    DisposeAEEventHandlerUPP(handler);
    return false;
  }
  bool result = false;
  const EventTypeSpec spec = {kEventClassAppleEvent, kEventAppleEvent};
  EventRef event_ref;
  if (ReceiveNextEvent(1, &spec, kEventDurationNanosecond, true, &event_ref) ==
      noErr) {
    OSStatus processed = AEProcessEvent(event_ref);
    ReleaseEvent(event_ref);
    result = processed == noErr;
  }
  AERemoveEventHandler(kInternetEventClass, kAEGetURL, handler, false);
  DisposeAEEventHandlerUPP(handler);
  return result;
}
