// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/dock.h"

#import <Foundation/Foundation.h>

#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace dock {

namespace {

NSDictionary* DictionaryFromPlistString(std::string_view plist_string) {
  return base::apple::ObjCCastStrict<NSDictionary>([NSPropertyListSerialization
      propertyListWithData:[base::SysUTF8ToNSString(plist_string)
                               dataUsingEncoding:NSUTF8StringEncoding]
                   options:NSPropertyListImmutable
                    format:nil
                     error:nil]);
}

}  // namespace

TEST(DockTest, FailureIfMissingKeys) {
  std::string plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
</dict>
</plist>
  )";

  NSDictionary* dock_plist = DictionaryFromPlistString(plist);
  RewriteDockPlistResult result =
      RewriteDockPlistForTesting(dock_plist, @"/Applications/Google Chrome.app",
                                 @"/Volumes/Google Chrome/Google Chrome.app");
  EXPECT_EQ(result.status, AddIconStatus::kFailure);
  EXPECT_EQ(result.result_plist, nil);
}

TEST(DockTest, NoChangeIfAlreadyPresent) {
  std::string plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>persistent-apps</key>
  <array>
    <dict>
      <key>GUID</key>
      <integer>3137035965</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.apps.launcher</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Applications/Apps.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>3137035966</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.Safari</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Volumes/Preboot/Cryptexes/App/System/Applications/Safari.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>621926469</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.google.Chrome</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///Applications/Google%20Chrome.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
  <key>recent-apps</key>
  <array/>
</dict>
</plist>
  )";

  NSDictionary* dock_plist = DictionaryFromPlistString(plist);
  RewriteDockPlistResult result =
      RewriteDockPlistForTesting(dock_plist, @"/Applications/Google Chrome.app",
                                 @"/Volumes/Google Chrome/Google Chrome.app");
  EXPECT_EQ(result.status, AddIconStatus::kAlreadyPresent);
  EXPECT_EQ(result.result_plist, nil);
}

TEST(DockTest, SwapsForInstalledDMGIfNotPresent) {
  std::string plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>persistent-apps</key>
  <array>
    <dict>
      <key>GUID</key>
      <integer>3137035965</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.apps.launcher</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Applications/Apps.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>621926469</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.google.Chrome</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///Volumes/Google%20Chrome/Google%20Chrome.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>3137035966</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.Safari</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Volumes/Preboot/Cryptexes/App/System/Applications/Safari.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
  <key>recent-apps</key>
  <array/>
</dict>
</plist>
  )";

  std::string result_plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>persistent-apps</key>
  <array>
    <dict>
      <key>GUID</key>
      <integer>3137035965</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.apps.launcher</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Applications/Apps.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>tile-data</key>
      <dict>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///Applications/Google%20Chrome.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>3137035966</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.Safari</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Volumes/Preboot/Cryptexes/App/System/Applications/Safari.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
  <key>recent-apps</key>
  <array/>
</dict>
</plist>
  )";

  NSDictionary* dock_plist = DictionaryFromPlistString(plist);
  RewriteDockPlistResult result =
      RewriteDockPlistForTesting(dock_plist, @"/Applications/Google Chrome.app",
                                 @"/Volumes/Google Chrome/Google Chrome.app");
  EXPECT_EQ(result.status, AddIconStatus::kSuccess);
  EXPECT_NSEQ(result.result_plist, DictionaryFromPlistString(result_plist));
}

TEST(DockTest, RemovesInstalledDMGIfBothPresent) {
  std::string plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>persistent-apps</key>
  <array>
    <dict>
      <key>GUID</key>
      <integer>3137035965</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.apps.launcher</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Applications/Apps.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>621926469</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.google.Chrome</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///Applications/Google%20Chrome.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>621926470</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.google.Chrome</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///Volumes/Google%20Chrome/Google%20Chrome.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>3137035966</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.Safari</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Volumes/Preboot/Cryptexes/App/System/Applications/Safari.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
  <key>recent-apps</key>
  <array/>
</dict>
</plist>
  )";

  std::string result_plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>persistent-apps</key>
  <array>
    <dict>
      <key>GUID</key>
      <integer>3137035965</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.apps.launcher</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Applications/Apps.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>621926469</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.google.Chrome</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///Applications/Google%20Chrome.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>3137035966</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.Safari</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Volumes/Preboot/Cryptexes/App/System/Applications/Safari.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
  <key>recent-apps</key>
  <array/>
</dict>
</plist>
  )";

  NSDictionary* dock_plist = DictionaryFromPlistString(plist);
  RewriteDockPlistResult result =
      RewriteDockPlistForTesting(dock_plist, @"/Applications/Google Chrome.app",
                                 @"/Volumes/Google Chrome/Google Chrome.app");
  EXPECT_EQ(result.status, AddIconStatus::kSuccess);
  EXPECT_NSEQ(result.result_plist, DictionaryFromPlistString(result_plist));
}

TEST(DockTest, PositionedAfterOtherWebBrowser) {
  std::string plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>persistent-apps</key>
  <array>
    <dict>
      <key>GUID</key>
      <integer>3137035966</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.Safari</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Volumes/Preboot/Cryptexes/App/System/Applications/Safari.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>3137035965</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.apps.launcher</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Applications/Apps.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
  <key>recent-apps</key>
  <array/>
</dict>
</plist>
  )";

  std::string result_plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>persistent-apps</key>
  <array>
    <dict>
      <key>GUID</key>
      <integer>3137035966</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.Safari</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Volumes/Preboot/Cryptexes/App/System/Applications/Safari.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>tile-data</key>
      <dict>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///Applications/Google%20Chrome.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>GUID</key>
      <integer>3137035965</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.apps.launcher</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Applications/Apps.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
  <key>recent-apps</key>
  <array/>
</dict>
</plist>
  )";

  NSDictionary* dock_plist = DictionaryFromPlistString(plist);
  RewriteDockPlistResult result =
      RewriteDockPlistForTesting(dock_plist, @"/Applications/Google Chrome.app",
                                 @"/Volumes/Google Chrome/Google Chrome.app");
  EXPECT_EQ(result.status, AddIconStatus::kSuccess);
  EXPECT_NSEQ(result.result_plist, DictionaryFromPlistString(result_plist));
}

TEST(DockTest, RemovesFromRecentApps) {
  std::string plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>persistent-apps</key>
  <array>
    <dict>
      <key>GUID</key>
      <integer>3137035965</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.apps.launcher</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Applications/Apps.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
  <key>recent-apps</key>
  <array>
    <dict>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.google.Chrome</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///Applications/Google%20Chrome.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
</dict>
</plist>
  )";

  std::string result_plist = R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>persistent-apps</key>
  <array>
    <dict>
      <key>GUID</key>
      <integer>3137035965</integer>
      <key>tile-data</key>
      <dict>
        <key>bundle-identifier</key>
        <string>com.apple.apps.launcher</string>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///System/Applications/Apps.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
    <dict>
      <key>tile-data</key>
      <dict>
        <key>file-data</key>
        <dict>
          <key>_CFURLString</key>
          <string>file:///Applications/Google%20Chrome.app/</string>
          <key>_CFURLStringType</key>
          <integer>15</integer>
        </dict>
      </dict>
      <key>tile-type</key>
      <string>file-tile</string>
    </dict>
  </array>
  <key>recent-apps</key>
  <array/>
</dict>
</plist>
  )";

  NSDictionary* dock_plist = DictionaryFromPlistString(plist);
  RewriteDockPlistResult result =
      RewriteDockPlistForTesting(dock_plist, @"/Applications/Google Chrome.app",
                                 @"/Volumes/Google Chrome/Google Chrome.app");
  EXPECT_EQ(result.status, AddIconStatus::kSuccess);
  EXPECT_NSEQ(result.result_plist, DictionaryFromPlistString(result_plist));
}

}  // namespace dock
