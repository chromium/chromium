// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/enterprise_util.h"

#import <OpenDirectory/OpenDirectory.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"

namespace base {

bool IsManagedDevice() {
  // MDM enrollment indicates the device is actively being managed. Simply being
  // joined to a domain, however, does not.
  base::MacDeviceManagementState mdm_state =
      base::IsDeviceRegisteredWithManagement();
  return mdm_state == base::MacDeviceManagementState::kLimitedMDMEnrollment ||
         mdm_state == base::MacDeviceManagementState::kFullMDMEnrollment ||
         mdm_state == base::MacDeviceManagementState::kDEPMDMEnrollment;
}

bool IsEnterpriseDevice() {
  // Domain join is a basic indicator of being an enterprise device.
  DeviceUserDomainJoinState join_state = AreDeviceAndUserJoinedToDomain();
  return join_state.device_joined || join_state.user_joined;
}

MacDeviceManagementState IsDeviceRegisteredWithManagement() {
  static MacDeviceManagementState state = [] {
    std::vector<std::string> profiles_argv{"/usr/bin/profiles", "status",
                                           "-type", "enrollment"};

    std::string profiles_stdout;
    if (!GetAppOutput(profiles_argv, &profiles_stdout)) {
      LOG(WARNING) << "Could not get profiles output.";
      return MacDeviceManagementState::kFailureAPIUnavailable;
    }

    // Sample output of `profiles` with full MDM enrollment:
    // Enrolled via DEP: Yes
    // MDM enrollment: Yes (User Approved)
    // MDM server: https://applemdm.example.com/some/path?foo=bar
    StringPairs property_states;
    if (!SplitStringIntoKeyValuePairs(profiles_stdout, ':', '\n',
                                      &property_states)) {
      return MacDeviceManagementState::kFailureUnableToParseResult;
    }

    bool enrolled_via_dep = false;
    bool mdm_enrollment_not_approved = false;
    bool mdm_enrollment_user_approved = false;

    for (const auto& property_state : property_states) {
      StringPiece property =
          TrimString(property_state.first, kWhitespaceASCII, TRIM_ALL);
      StringPiece state =
          TrimString(property_state.second, kWhitespaceASCII, TRIM_ALL);

      if (property == "Enrolled via DEP") {
        if (state == "Yes") {
          enrolled_via_dep = true;
        } else if (state != "No") {
          return MacDeviceManagementState::kFailureUnableToParseResult;
        }
      } else if (property == "MDM enrollment") {
        if (state == "Yes") {
          mdm_enrollment_not_approved = true;
        } else if (state == "Yes (User Approved)") {
          mdm_enrollment_user_approved = true;
        } else if (state != "No") {
          return MacDeviceManagementState::kFailureUnableToParseResult;
        }
      } else {
        // Ignore any other output lines, for future extensibility.
      }
    }

    if (!enrolled_via_dep && !mdm_enrollment_not_approved &&
        !mdm_enrollment_user_approved) {
      return MacDeviceManagementState::kNoEnrollment;
    }

    if (!enrolled_via_dep && mdm_enrollment_not_approved &&
        !mdm_enrollment_user_approved) {
      return MacDeviceManagementState::kLimitedMDMEnrollment;
    }

    if (!enrolled_via_dep && !mdm_enrollment_not_approved &&
        mdm_enrollment_user_approved) {
      return MacDeviceManagementState::kFullMDMEnrollment;
    }

    if (enrolled_via_dep && !mdm_enrollment_not_approved &&
        mdm_enrollment_user_approved) {
      return MacDeviceManagementState::kDEPMDMEnrollment;
    }

    return MacDeviceManagementState::kFailureUnableToParseResult;
  }();

  return state;
}

DeviceUserDomainJoinState AreDeviceAndUserJoinedToDomain() {
  static DeviceUserDomainJoinState state = [] {
    DeviceUserDomainJoinState state{false, false};

    @autoreleasepool {
      ODSession* session = [ODSession defaultSession];
      if (session == nil) {
        DLOG(WARNING) << "ODSession default session is nil.";
        return state;
      }

      NSError* error = nil;

      NSArray<NSString*>* all_node_names =
          [session nodeNamesAndReturnError:&error];
      if (!all_node_names) {
        DLOG(WARNING) << "ODSession failed to give node names: "
                      << error.localizedDescription.UTF8String;
        return state;
      }

      NSUInteger num_nodes = all_node_names.count;
      if (num_nodes < 3) {
        DLOG(WARNING) << "ODSession returned too few node names: "
                      << all_node_names.description.UTF8String;
        return state;
      }

      if (num_nodes > 3) {
        // Non-enterprise machines have:"/Search", "/Search/Contacts",
        // "/Local/Default". Everything else would be enterprise management.
        state.device_joined = true;
      }

      ODNode* node = [ODNode nodeWithSession:session
                                        type:kODNodeTypeAuthentication
                                       error:&error];
      if (node == nil) {
        DLOG(WARNING) << "ODSession cannot obtain the authentication node: "
                      << error.localizedDescription.UTF8String;
        return state;
      }

      // Now check the currently logged on user.
      ODQuery* query = [ODQuery queryWithNode:node
                               forRecordTypes:kODRecordTypeUsers
                                    attribute:kODAttributeTypeRecordName
                                    matchType:kODMatchEqualTo
                                  queryValues:NSUserName()
                             returnAttributes:kODAttributeTypeAllAttributes
                               maximumResults:0
                                        error:&error];
      if (query == nil) {
        DLOG(WARNING) << "ODSession cannot create user query: "
                      << error.localizedDescription.UTF8String;
        return state;
      }

      NSArray* results = [query resultsAllowingPartial:NO error:&error];
      if (!results) {
        DLOG(WARNING) << "ODSession cannot obtain current user node: "
                      << error.localizedDescription.UTF8String;
        return state;
      }

      if (results.count != 1) {
        DLOG(WARNING) << @"ODSession unexpected number of user nodes: "
                      << results.count;
      }

      for (id element in results) {
        ODRecord* record = mac::ObjCCastStrict<ODRecord>(element);
        NSArray* attributes =
            [record valuesForAttribute:kODAttributeTypeMetaRecordName
                                 error:nil];
        for (id attribute in attributes) {
          NSString* attribute_value = mac::ObjCCastStrict<NSString>(attribute);
          // Example: "uid=johnsmith,ou=People,dc=chromium,dc=org
          NSRange domain_controller =
              [attribute_value rangeOfString:@"(^|,)\\s*dc="
                                     options:NSRegularExpressionSearch];
          if (domain_controller.length > 0) {
            state.user_joined = true;
          }
        }

        // Scan alternative identities.
        attributes =
            [record valuesForAttribute:kODAttributeTypeAltSecurityIdentities
                                 error:nil];
        for (id attribute in attributes) {
          NSString* attribute_value = mac::ObjCCastStrict<NSString>(attribute);
          NSRange icloud =
              [attribute_value rangeOfString:@"CN=com.apple.idms.appleid.prd"
                                     options:NSCaseInsensitiveSearch];
          if (!icloud.length) {
            // Any alternative identity that is not iCloud is likely enterprise
            // management.
            state.user_joined = true;
          }
        }
      }
    }

    return state;
  }();

  return state;
}

}  // namespace base
