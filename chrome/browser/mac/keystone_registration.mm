// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/keystone_registration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace keystone_registration {

// Definitions of the Keystone registration constants needed here. From
// KSRegistration.m.

NSString* KSRegistrationVersionKey = @"Version";
NSString* KSRegistrationExistenceCheckerTypeKey = @"ExistenceCheckerType";
NSString* KSRegistrationExistenceCheckerStringKey = @"ExistenceCheckerString";
NSString* KSRegistrationServerURLStringKey = @"URLString";
NSString* KSRegistrationPreserveTrustedTesterTokenKey = @"PreserveTTT";
NSString* KSRegistrationTagKey = @"Tag";
NSString* KSRegistrationTagPathKey = @"TagPath";
NSString* KSRegistrationTagKeyKey = @"TagKey";
NSString* KSRegistrationBrandPathKey = @"BrandPath";
NSString* KSRegistrationBrandKeyKey = @"BrandKey";
NSString* KSRegistrationVersionPathKey = @"VersionPath";
NSString* KSRegistrationVersionKeyKey = @"VersionKey";

NSString* KSRegistrationDidCompleteNotification =
    @"KSRegistrationDidCompleteNotification";
NSString* KSRegistrationPromotionDidCompleteNotification =
    @"KSRegistrationPromotionDidCompleteNotification";

NSString* KSRegistrationCheckForUpdateNotification =
    @"KSRegistrationCheckForUpdateNotification";
NSString* KSRegistrationStatusKey = @"Status";
NSString* KSRegistrationUpdateCheckErrorKey = @"Error";
NSString* KSRegistrationUpdateCheckRawResultsKey = @"RawResults";
NSString* KSRegistrationUpdateCheckRawErrorMessagesKey = @"RawErrorMessages";

NSString* KSRegistrationStartUpdateNotification =
    @"KSRegistrationStartUpdateNotification";
NSString* KSUpdateCheckSuccessfulKey = @"CheckSuccessful";
NSString* KSUpdateCheckSuccessfullyInstalledKey = @"SuccessfullyInstalled";

NSString* KSRegistrationRemoveExistingTag = @"";

NSString* KSReportingAttributeValueKey = @"value";
NSString* KSReportingAttributeExpirationDateKey = @"expiresAt";
NSString* KSReportingAttributeAggregationTypeKey = @"aggregation";

}  // namespace keystone_registration
