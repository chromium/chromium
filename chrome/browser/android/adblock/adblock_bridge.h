/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CHROME_BROWSER_ANDROID_ADBLOCK_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_ADBLOCK_BRIDGE_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/prefs/pref_member.h"
#include "base/lazy_instance.h"

class PrefService;

class AdblockBridge {
 public:
   static bool prefs_moved_to_thread;
   static BooleanPrefMember* enable_adblock;
   static StringListPrefMember* adblock_whitelisted_domains;
   
   static void InitializePrefsOnUIThread(PrefService* pref_service);
   static void ReleasePrefs();

   static jlong getFilterEnginePtr(); // thread-safe
   static void setFilterEnginePtr(jlong ptr); // thread-safe

  private:
    static base::LazyInstance<base::Lock>::DestructorAtExit filterEnginePtrLock;
    static jlong filterEnginePtr;
};

#endif  // CHROME_BROWSER_ANDROID_ADBLOCK_BRIDGE_H_
