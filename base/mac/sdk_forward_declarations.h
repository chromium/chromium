// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains forward declarations for items in later SDKs than the
// default one with which Chromium is built (currently 10.10).
// If you call any function from this header, be sure to check at runtime for
// respondsToSelector: before calling these functions (else your code will crash
// on older OS X versions that chrome still supports).

#ifndef BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
#define BASE_MAC_SDK_FORWARD_DECLARATIONS_H_

#import <AppKit/AppKit.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreWLAN/CoreWLAN.h>
#import <IOBluetooth/IOBluetooth.h>
#import <ImageCaptureCore/ImageCaptureCore.h>
#import <QuartzCore/QuartzCore.h>
#include <stdint.h>

#include "base/base_export.h"
#include "base/mac/availability.h"

// ----------------------------------------------------------------------------
// Define typedefs, enums, and protocols not available in the version of the
// OSX SDK being compiled against.
// ----------------------------------------------------------------------------

#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11

enum {
  NSPressureBehaviorUnknown = -1,
  NSPressureBehaviorPrimaryDefault = 0,
  NSPressureBehaviorPrimaryClick = 1,
  NSPressureBehaviorPrimaryGeneric = 2,
  NSPressureBehaviorPrimaryAccelerator = 3,
  NSPressureBehaviorPrimaryDeepClick = 5,
  NSPressureBehaviorPrimaryDeepDrag = 6
};
typedef NSInteger NSPressureBehavior;

@interface NSPressureConfiguration : NSObject
- (instancetype)initWithPressureBehavior:(NSPressureBehavior)pressureBehavior;
@end

enum {
  NSSpringLoadingHighlightNone = 0,
  NSSpringLoadingHighlightStandard,
  NSSpringLoadingHighlightEmphasized
};
typedef NSUInteger NSSpringLoadingHighlight;

#endif  // MAC_OS_X_VERSION_10_11

#if !defined(MAC_OS_X_VERSION_10_12) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12

// The protocol was formalized by the 10.12 SDK, but it was informally used
// before.
@protocol CAAnimationDelegate
- (void)animationDidStart:(CAAnimation*)animation;
- (void)animationDidStop:(CAAnimation*)animation finished:(BOOL)finished;
@end

@protocol CALayerDelegate
@end

#endif  // MAC_OS_X_VERSION_10_12

// ----------------------------------------------------------------------------
// Define NSStrings only available in newer versions of the OSX SDK to force
// them to be statically linked.
// ----------------------------------------------------------------------------

extern "C" {
#if !defined(MAC_OS_X_VERSION_10_10) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_10
BASE_EXPORT extern NSString* const CIDetectorTypeQRCode;
BASE_EXPORT extern NSString* const NSUserActivityTypeBrowsingWeb;
BASE_EXPORT extern NSString* const NSAppearanceNameVibrantDark;
BASE_EXPORT extern NSString* const NSAppearanceNameVibrantLight;
#endif  // MAC_OS_X_VERSION_10_10
}  // extern "C"

// ----------------------------------------------------------------------------
// If compiling against an older version of the OSX SDK, declare classes and
// functions that are available in newer versions of the OSX SDK. If compiling
// against a newer version of the OSX SDK, redeclare those same classes and
// functions to suppress -Wpartial-availability warnings.
// ----------------------------------------------------------------------------

// Once Chrome no longer supports OSX 10.9, everything within this preprocessor
// block can be removed.
#if !defined(MAC_OS_X_VERSION_10_10) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_10

@interface NSUserActivity (YosemiteSDK)
@property(readonly, copy) NSString* activityType;
@property(copy) NSDictionary* userInfo;
@property(copy) NSURL* webpageURL;
@property(copy) NSString* title;
- (instancetype)initWithActivityType:(NSString*)activityType;
- (void)becomeCurrent;
- (void)invalidate;
@end

@interface CBUUID (YosemiteSDK)
- (NSString*)UUIDString;
@end

@interface NSViewController (YosemiteSDK)
- (void)viewDidLoad;
@end

@interface NSWindow (YosemiteSDK)
- (void)setTitlebarAppearsTransparent:(BOOL)flag;
@end

@interface NSProcessInfo (YosemiteSDK)
@property(readonly) NSOperatingSystemVersion operatingSystemVersion;
@end

@interface NSLayoutConstraint (YosemiteSDK)
@property(getter=isActive) BOOL active;
+ (void)activateConstraints:(NSArray*)constraints;
@end

@interface NSVisualEffectView (YosemiteSDK)
- (void)setState:(NSVisualEffectState)state;
@end

@class NSVisualEffectView;

@interface CIQRCodeFeature (YosemiteSDK)
@property(readonly) CGRect bounds;
@property(readonly) CGPoint topLeft;
@property(readonly) CGPoint topRight;
@property(readonly) CGPoint bottomLeft;
@property(readonly) CGPoint bottomRight;
@property(readonly, copy) NSString* messageString;
@end

@class CIQRCodeFeature;

@interface NSView (YosemiteSDK)
- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector;
@property(copy) NSString* accessibilityLabel;
@end

#endif  // MAC_OS_X_VERSION_10_10

// Once Chrome no longer supports OSX 10.10.2, everything within this
// preprocessor block can be removed.
#if !defined(MAC_OS_X_VERSION_10_10_3) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_10_3

@interface NSEvent (Yosemite_3_SDK)
@property(readonly) NSInteger stage;
@end

#endif  // MAC_OS_X_VERSION_10_10

// ----------------------------------------------------------------------------
// Define NSStrings only available in newer versions of the OSX SDK to force
// them to be statically linked.
// ----------------------------------------------------------------------------

extern "C" {
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_11
BASE_EXPORT extern NSString* const CIDetectorTypeText;
#endif  // MAC_OS_X_VERSION_10_11
}  // extern "C"

// Once Chrome no longer supports OSX 10.10, everything within this
// preprocessor block can be removed.
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_11

@class NSLayoutDimension;
@class NSLayoutXAxisAnchor;
@class NSLayoutYAxisAnchor;

@interface NSObject (ElCapitanSDK)
- (NSLayoutConstraint*)constraintEqualToConstant:(CGFloat)c;
- (NSLayoutConstraint*)constraintGreaterThanOrEqualToConstant:(CGFloat)c;
@end

@interface NSView (ElCapitanSDK)
- (void)setPressureConfiguration:(NSPressureConfiguration*)aConfiguration
    API_AVAILABLE(macos(10.11));
@property(readonly, strong)
    NSLayoutXAxisAnchor* leftAnchor API_AVAILABLE(macos(10.11));
@property(readonly, strong)
    NSLayoutXAxisAnchor* rightAnchor API_AVAILABLE(macos(10.11));
@property(readonly, strong)
    NSLayoutYAxisAnchor* bottomAnchor API_AVAILABLE(macos(10.11));
@property(readonly, strong)
    NSLayoutDimension* widthAnchor API_AVAILABLE(macos(10.11));
@end

@interface NSWindow (ElCapitanSDK)
- (void)performWindowDragWithEvent:(NSEvent*)event;
@end

@interface CIRectangleFeature (ElCapitanSDK)
@property(readonly) CGRect bounds;
@end

@class CIRectangleFeature;

#endif  // MAC_OS_X_VERSION_10_11

// Once Chrome no longer supports OSX 10.11, everything within this
// preprocessor block can be removed.
#if !defined(MAC_OS_X_VERSION_10_12) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12

@interface NSWindow (SierraSDK)
@property(class) BOOL allowsAutomaticWindowTabbing;
@end

#endif  // MAC_OS_X_VERSION_10_12

// Once Chrome no longer supports OSX 10.12.0, everything within this
// preprocessor block can be removed.
#if !defined(MAC_OS_X_VERSION_10_12_1) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12_1

@interface NSButton (SierraPointOneSDK)
@property(copy) NSColor* bezelColor;
@property BOOL imageHugsTitle;
+ (instancetype)buttonWithTitle:(NSString*)title
                         target:(id)target
                         action:(SEL)action;
+ (instancetype)buttonWithImage:(NSImage*)image
                         target:(id)target
                         action:(SEL)action;
+ (instancetype)buttonWithTitle:(NSString*)title
                          image:(NSImage*)image
                         target:(id)target
                         action:(SEL)action;
@end

@interface NSSegmentedControl (SierraPointOneSDK)
+ (instancetype)segmentedControlWithImages:(NSArray*)images
                              trackingMode:(NSSegmentSwitchTracking)trackingMode
                                    target:(id)target
                                    action:(SEL)action;
@end

@interface NSTextField (SierraPointOneSDK)
+ (instancetype)labelWithAttributedString:
    (NSAttributedString*)attributedStringValue;
@end

#endif  // MAC_OS_X_VERSION_10_12_1

// Once Chrome no longer supports OSX 10.12, everything within this
// preprocessor block can be removed.
#if !defined(MAC_OS_X_VERSION_10_13) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_13

// VNRequest forward declarations.
@class VNRequest;
typedef void (^VNRequestCompletionHandler)(VNRequest* request, NSError* error);

@interface VNRequest : NSObject<NSCopying>
- (instancetype)initWithCompletionHandler:
    (VNRequestCompletionHandler)completionHandler NS_DESIGNATED_INITIALIZER;
@property(readonly, nonatomic, copy) NSArray* results;
@end

// VNDetectFaceLandmarksRequest forward declarations.
@interface VNImageBasedRequest : VNRequest
@end

@protocol VNFaceObservationAccepting<NSObject>
@end

@interface VNDetectFaceLandmarksRequest
    : VNImageBasedRequest<VNFaceObservationAccepting>
@end

// VNImageRequestHandler forward declarations.
typedef NSString* VNImageOption NS_STRING_ENUM;

@interface VNImageRequestHandler : NSObject
- (instancetype)initWithCIImage:(CIImage*)image
                        options:(NSDictionary<VNImageOption, id>*)options;
- (BOOL)performRequests:(NSArray<VNRequest*>*)requests error:(NSError**)error;
@end

// VNFaceLandmarks2D forward declarations.
@interface VNFaceLandmarkRegion : NSObject
@property(readonly) NSUInteger pointCount;
@end

@interface VNFaceLandmarkRegion2D : VNFaceLandmarkRegion
@property(readonly, assign)
    const CGPoint* normalizedPoints NS_RETURNS_INNER_POINTER;
@end

@interface VNFaceLandmarks2D : NSObject
@property(readonly) VNFaceLandmarkRegion2D* leftEye;
@property(readonly) VNFaceLandmarkRegion2D* rightEye;
@property(readonly) VNFaceLandmarkRegion2D* outerLips;
@property(readonly) VNFaceLandmarkRegion2D* nose;
@end

// VNFaceObservation forward declarations.
@interface VNObservation : NSObject<NSCopying, NSSecureCoding>
@end

@interface VNDetectedObjectObservation : VNObservation
@property(readonly, nonatomic, assign) CGRect boundingBox;
@end

@interface VNFaceObservation : VNDetectedObjectObservation
@property(readonly, nonatomic, strong) VNFaceLandmarks2D* landmarks;
@end

// VNDetectBarcodesRequest forward declarations.
typedef NSString* VNBarcodeSymbology NS_STRING_ENUM;

@interface VNDetectBarcodesRequest : VNImageBasedRequest
@property(readwrite, nonatomic, copy) NSArray<VNBarcodeSymbology>* symbologies;
@end

// VNBarcodeObservation forward declarations.
@interface VNRectangleObservation : VNDetectedObjectObservation
@property(readonly, nonatomic, assign) CGPoint topLeft;
@property(readonly, nonatomic, assign) CGPoint topRight;
@property(readonly, nonatomic, assign) CGPoint bottomLeft;
@property(readonly, nonatomic, assign) CGPoint bottomRight;
@end

@interface VNBarcodeObservation : VNRectangleObservation
@property(readonly, nonatomic, copy) NSString* payloadStringValue;
@property(readonly, nonatomic, copy) VNBarcodeSymbology symbology;
@end

#endif  // MAC_OS_X_VERSION_10_13

// Once Chrome no longer supports macOS 10.13, everything within this
// preprocessor block can be removed.
#if !defined(MAC_OS_X_VERSION_10_14) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_14

typedef NSString* NSAppearanceName;

@interface NSApplication (ForwardDeclare)
@property(strong) NSAppearance* appearance;
@property(readonly, strong) NSAppearance* effectiveAppearance;
@end

@interface NSAppearance (ForwardDeclare)
- (NSAppearanceName)bestMatchFromAppearancesWithNames:
    (NSArray<NSAppearanceName>*)appearances;
@end

BASE_EXPORT extern NSAppearanceName const NSAppearanceNameDarkAqua;

#endif  // MAC_OS_X_VERSION_10_14

#if !defined(MAC_OS_X_VERSION_10_15) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_15

@interface NSScreen (ForwardDeclare)
@property(readonly)
    CGFloat maximumPotentialExtendedDynamicRangeColorComponentValue;
@end

@interface SFUniversalLink : NSObject
- (instancetype)initWithWebpageURL:(NSURL*)url;
@property(readonly) NSURL* webpageURL;
@property(readonly) NSURL* applicationURL;
@property(getter=isEnabled) BOOL enabled;
@end

#endif  // MAC_OS_X_VERSION_10_15

// ----------------------------------------------------------------------------
// The symbol for kCWSSIDDidChangeNotification is available in the
// CoreWLAN.framework for OSX versions 10.6 through 10.10. The symbol is not
// declared in the OSX 10.9+ SDK, so when compiling against an OSX 10.9+ SDK,
// declare the symbol.
// ----------------------------------------------------------------------------
BASE_EXPORT extern "C" NSString* const kCWSSIDDidChangeNotification;

// Once Chrome is built with at least the macOS 10.13 SDK, everything within
// this preprocessor block can be removed.
#if !defined(MAC_OS_X_VERSION_10_13)
typedef NSString* NSTextCheckingOptionKey;
typedef NSString* NSAccessibilityRole;
typedef NSString* NSAccessibilitySubrole;
#endif

#endif  // BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
