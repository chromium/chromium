// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_WRAP_CG_DISPLAY_H_
#define BASE_MAC_WRAP_CG_DISPLAY_H_

// All these symbols have incorrect availability annotations in the 13.3 SDK.
// These have the correct annotation. See https://crbug.com/1431897.
// TODO(thakis): Remove this once FB12109479 is fixed and we updated to an SDK
// with the fix.

#include <CoreGraphics/CoreGraphics.h>

inline CGDisplayStreamRef __nullable wrapCGDisplayStreamCreate(
    CGDirectDisplayID display,
    size_t outputWidth,
    size_t outputHeight,
    int32_t pixelFormat,
    CFDictionaryRef __nullable properties,
    CGDisplayStreamFrameAvailableHandler __nullable handler)
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "Please use ScreenCaptureKit API's "
        "initWithFilter:configuration:delegate: instead") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return CGDisplayStreamCreate(display, outputWidth, outputHeight, pixelFormat,
                               properties, handler);
#pragma clang diagnostic pop
}

inline CFRunLoopSourceRef __nullable wrapCGDisplayStreamGetRunLoopSource(
    CGDisplayStreamRef cg_nullable displayStream)
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "There is no direct replacement for this function. Please use "
        "ScreenCaptureKit API's SCStream to replace CGDisplayStream") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return CGDisplayStreamGetRunLoopSource(displayStream);
#pragma clang diagnostic pop
}

inline CGError wrapCGDisplayStreamStart(
    CGDisplayStreamRef cg_nullable displayStream)
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "Please use ScreenCaptureKit API's "
        "startCaptureWithCompletionHandler: to start a stream instead") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return CGDisplayStreamStart(displayStream);
#pragma clang diagnostic pop
}

inline CGError wrapCGDisplayStreamStop(
    CGDisplayStreamRef cg_nullable displayStream)
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "Please use ScreenCaptureKit API's "
        "stopCaptureWithCompletionHandler: to stop a stream instead") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return CGDisplayStreamStop(displayStream);
#pragma clang diagnostic pop
}

inline _Null_unspecified CFStringRef wrapkCGDisplayStreamColorSpace()
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "Please use ScreenCaptureKit API's SCStreamConfiguration "
        "colorSpaceName property instead") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return kCGDisplayStreamColorSpace;
#pragma clang diagnostic pop
}

inline _Null_unspecified CFStringRef wrapkCGDisplayStreamDestinationRect()
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "Please use ScreenCaptureKit API's SCStreamConfiguration "
        "destinationRect property instead") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return kCGDisplayStreamDestinationRect;
#pragma clang diagnostic pop
}

inline _Null_unspecified CFStringRef wrapkCGDisplayStreamMinimumFrameTime()
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "Please use ScreenCaptureKit API's SCStreamConfiguration "
        "minimumFrameInterval property instead") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return kCGDisplayStreamMinimumFrameTime;
#pragma clang diagnostic pop
}

inline _Null_unspecified CFStringRef wrapkCGDisplayStreamPreserveAspectRatio()
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "Please use ScreenCaptureKit API's SCStreamConfiguration "
        "preserveAspectRatio property instead") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return kCGDisplayStreamPreserveAspectRatio;
#pragma clang diagnostic pop
}

inline _Null_unspecified CFStringRef wrapkCGDisplayStreamShowCursor()
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "Please use ScreenCaptureKit API's SCStreamConfiguration showsCursor "
        "property instead") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return kCGDisplayStreamShowCursor;
#pragma clang diagnostic pop
}

inline const CGRect* __nullable
wrapCGDisplayStreamUpdateGetRects(CGDisplayStreamUpdateRef __nullable updateRef,
                                  CGDisplayStreamUpdateRectType rectType,
                                  size_t* _Null_unspecified rectCount)
    CG_AVAILABLE_BUT_DEPRECATED(
        10.8,
        14.0,
        "Please use ScreenCaptureKit API's SCStreamFrameInfo with "
        "SCStreamFrameInfoContentRect instead") {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return CGDisplayStreamUpdateGetRects(updateRef, rectType, rectCount);
#pragma clang diagnostic pop
}

#endif  // BASE_MAC_WRAP_CG_DISPLAY_H_
