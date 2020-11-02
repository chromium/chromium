// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/keychain_data_helper_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"

namespace extensions {
namespace {

// Creates an access for a generic password item to share it with other Google
// applications with teamid:EQHXZ8M8AV (taken from the signing certificate).
OSStatus CreateTargetAccess(NSString* service_name, SecAccessRef* access_ref) {
  OSStatus status = noErr;
  status =
      SecAccessCreate(base::mac::NSToCFCast(service_name), nullptr, access_ref);
  if (status != noErr)
    return status;

  base::ScopedCFTypeRef<CFArrayRef> acl_list;
  status = SecAccessCopyACLList(*access_ref, acl_list.InitializeInto());
  if (status != noErr)
    return status;

  for (id acl in base::mac::CFToNSCast(acl_list.get())) {
    base::ScopedCFTypeRef<CFArrayRef> app_list;
    base::ScopedCFTypeRef<CFStringRef> description;
    SecKeychainPromptSelector dummy_prompt_selector;
    status = SecACLCopyContents(
        static_cast<SecACLRef>(acl), app_list.InitializeInto(),
        description.InitializeInto(), &dummy_prompt_selector);
    if (status != noErr)
      return status;

    NSArray* const ns_app_list = base::mac::CFToNSCast(app_list);
    // Replace explicit non-empty app list with void to allow any application
    if (ns_app_list && ns_app_list.count != 0) {
      status = SecACLSetContents(static_cast<SecACLRef>(acl), nullptr,
                                 description, dummy_prompt_selector);
      if (status != noErr)
        return status;
    }
  }

  return noErr;
}

// Checks if |item| is accessible by other applications with same teamid.
// Returns true if the item has to be recreated in order to assign proper ACL.
// Returns false otherwise, or in case there was an error getting the ACL
// information for analysis.
OSStatus NeedToRecreateKeychainItem(SecKeychainItemRef item_ref, bool* result) {
  DCHECK(result);
  *result = false;
  OSStatus status = noErr;
  base::ScopedCFTypeRef<SecAccessRef> access_ref;
  status = SecKeychainItemCopyAccess(item_ref, access_ref.InitializeInto());
  if (status != noErr)
    return status;

  base::ScopedCFTypeRef<CFArrayRef> acl_list;
  status = SecAccessCopyACLList(access_ref.get(), acl_list.InitializeInto());
  if (status != noErr)
    return status;

  for (id acl in base::mac::CFToNSCast(acl_list.get())) {
    for (id tag in base::mac::CFToNSCast(
             SecACLCopyAuthorizations(static_cast<SecACLRef>(acl)))) {
      if ([base::mac::ObjCCastStrict<NSString>(tag)
              isEqualToString:base::mac::CFToNSCast(
                                  kSecACLAuthorizationExportClear)]) {
        base::ScopedCFTypeRef<CFArrayRef> app_list;
        base::ScopedCFTypeRef<CFStringRef> description;
        SecKeychainPromptSelector dummy_prompt_selector;
        status = SecACLCopyContents(
            static_cast<SecACLRef>(acl), app_list.InitializeInto(),
            description.InitializeInto(), &dummy_prompt_selector);
        if (status != noErr)
          return status;
        NSArray* const ns_app_list = base::mac::CFToNSCast(app_list);
        // We need an empty app list here to allow access by all applications
        if (ns_app_list && ns_app_list.count != 0) {
          *result = true;
          return noErr;
        }
      }
    }
  }
  return noErr;
}

OSStatus DeleteKeychainItem(SecKeychainItemRef item_ref) {
  SecKeychainItemRef item_refs[] = {item_ref};
  base::ScopedCFTypeRef<CFArrayRef> item_match(
      CFArrayCreate(nullptr, reinterpret_cast<const void**>(&item_refs),
                    base::size(item_refs), &kCFTypeArrayCallBacks));
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
  CFDictionarySetValue(query, kSecMatchItemList, item_match);

  return SecItemDelete(query);
}

}  // namespace

OSStatus RecreateKeychainItemIfNecessary(const std::string& service_name,
                                         const std::string& account_name,
                                         const std::string& password,
                                         SecKeychainItemRef item_ref,
                                         bool* keep_password) {
  DCHECK(keep_password);
  *keep_password = true;
  OSStatus status = noErr;

  bool need_recreate;
  status = NeedToRecreateKeychainItem(item_ref, &need_recreate);
  if (!need_recreate)
    return status;

  status = DeleteKeychainItem(item_ref);
  if (status != noErr)
    return status;

  status = WriteKeychainItem(service_name, account_name, password);
  if (status != noErr)
    *keep_password = false;

  return status;
}

OSStatus WriteKeychainItem(const std::string& service_name,
                           const std::string& account_name,
                           const std::string& password) {
  SecKeychainAttribute attributes[] = {
      {kSecLabelItemAttr, static_cast<UInt32>(service_name.length()),
       const_cast<char*>(service_name.data())},
      {kSecDescriptionItemAttr, 0U, nullptr},
      {kSecGenericItemAttr, 0U, nullptr},
      {kSecCommentItemAttr, 0U, nullptr},
      {kSecServiceItemAttr, static_cast<UInt32>(service_name.length()),
       const_cast<char*>(service_name.data())},
      {kSecAccountItemAttr, static_cast<UInt32>(account_name.length()),
       const_cast<char*>(account_name.data())}};
  SecKeychainAttributeList attribute_list = {base::size(attributes),
                                             attributes};

  base::ScopedCFTypeRef<SecAccessRef> access_ref;
  OSStatus status = CreateTargetAccess(base::SysUTF8ToNSString(service_name),
                                       access_ref.InitializeInto());
  if (status != noErr)
    return status;
  return SecKeychainItemCreateFromContent(
      kSecGenericPasswordItemClass, &attribute_list,
      static_cast<UInt32>(password.size()), password.data(), nullptr,
      access_ref.get(), nullptr);
}

}  // namespace extensions
