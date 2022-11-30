// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_USER_DATA_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_USER_DATA_H_

#include "base/supports_user_data.h"
#include "chrome/browser/android/tab_android.h"

// A base class for classes attached to, and scoped to, the lifetime of a
// TabAndroid. For example:
//
// --- in foo_tab_helper.h ---
// class FooTabHelper : public web::TabAndroidUserData<FooTabHelper> {
//  public:
//   ~FooTabHelper() override;
//   // ... more public stuff here ...
//  private:
//   explicit FooTabHelper(web::TabAndroid* tab);
//   friend class web::TabAndroidUserData<FooTabHelper>;
//   TAB_ANDROID_USER_DATA_KEY_DECL();
//   // ... more private stuff here ...
// };
//
// --- in foo_tab_helper.cc ---
// TAB_ANDROID_USER_DATA_KEY_IMPL(FooTabHelper)
template <typename T>
class TabAndroidUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified TabAndroid.
  // If an instance is already attached, does nothing.
  static void CreateForTabAndroid(TabAndroid* tab) {
    DCHECK(tab);
    if (!FromTabAndroid(tab))
      tab->SetUserData(UserDataKey(), base::WrapUnique(new T(tab)));
  }

  // Retrieves the instance of type T that was attached to the specified
  // TabAndroid (via CreateForTabAndroid above) and returns it. If no instance
  // of the type was attached, returns nullptr.
  static T* FromTabAndroid(TabAndroid* tab) {
    DCHECK(tab);
    return static_cast<T*>(tab->GetUserData(UserDataKey()));
  }

  static const void* UserDataKey() { return &T::kUserDataKey; }
};

// This macro declares a static variable inside the class that inherits from
// TabAndroidUserData The address of this static variable is used as the key to
// store/retrieve an instance of the class on/from a TabAndroid.
#define TAB_ANDROID_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define TAB_ANDROID_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey;

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_USER_DATA_H_
