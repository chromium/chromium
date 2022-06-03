// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/objc_release_properties.h"

#include <memory>

#include <objc/runtime.h>

#include "base/check.h"
#include "base/memory/free_deleter.h"

namespace {

bool IsRetained(objc_property_t property) {
  // The format of the string returned by property_getAttributes is documented
  // at
  // http://developer.apple.com/library/mac/#documentation/Cocoa/Conceptual/ObjCRuntimeGuide/Articles/ocrtPropertyIntrospection.html#//apple_ref/doc/uid/TP40008048-CH101-SW6
  const char* attribute = property_getAttributes(property);
  while (attribute[0]) {
    switch (attribute[0]) {
      case 'C':  // copy
      case '&':  // retain
        return true;
    }
    do {
      attribute++;
    } while (attribute[0] && attribute[-1] != ',');
  }
  return false;
}

id ValueOf(id obj, objc_property_t property) {
  std::unique_ptr<char, base::FreeDeleter> ivar_name(
      property_copyAttributeValue(property, "V"));  // instance variable name
  if (!ivar_name)
    return nil;
  id ivar_value = nil;
  Ivar ivar = object_getInstanceVariable(obj, &*ivar_name,
                                         reinterpret_cast<void**>(&ivar_value));
  DCHECK(ivar);
  return ivar_value;
}

}  // namespace

namespace base {
namespace mac {
namespace details {

void ReleaseProperties(id self, Class cls) {
  unsigned int property_count;
  std::unique_ptr<objc_property_t[], base::FreeDeleter> properties(
      class_copyPropertyList(cls, &property_count));
  for (size_t i = 0; i < property_count; ++i) {
    objc_property_t property = properties[i];
    if (!IsRetained(property))
      continue;
    [ValueOf(self, property) release];
  }
}

}  // namespace details
}  // namespace mac
}  // namespace base
