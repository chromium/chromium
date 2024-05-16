#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

PREFIX="$1"


# The expected structures for CTS archives are like:
#   - (x86|arm64)/<dessert letter>
TARGET_DIR="$PREFIX/arm64/O"
IN_ZIPFILE=android-cts-8.0_R26-linux_x86-arm.zip
OUT_ZIPFILE=android-cts-arm64-8.0_r26.zip

# The CIPD source is a collection of zip files containing all CTS tests
# The below is what we
# need for filtered archive package.
mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/x86/O"
IN_ZIPFILE=android-cts-8.0_R26-linux_x86-x86.zip
OUT_ZIPFILE=android-cts-x86-8.0_r26.zip

mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/arm64/P"
IN_ZIPFILE=android-cts-9.0_r20-linux_x86-arm.zip
OUT_ZIPFILE=android-cts-arm64-9.0_r20.zip

mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/x86/P"
IN_ZIPFILE=android-cts-9.0_r20-linux_x86-x86.zip
OUT_ZIPFILE=android-cts-x86-9.0_r20.zip

mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/arm64/Q"
IN_ZIPFILE=android-cts-10_r16-linux_x86-arm.zip
OUT_ZIPFILE=android-cts-arm64-10_r16.zip

mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/x86/Q"
IN_ZIPFILE=android-cts-10_r16-linux_x86-x86.zip
OUT_ZIPFILE=android-cts-x86-10_r16.zip

mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/arm64/R"
IN_ZIPFILE=android-cts-11_r15-linux_x86-arm.zip
OUT_ZIPFILE=android-cts-arm64-11_r15.zip

mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsInputMethodTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsMockInputMethod.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAutoFillServiceTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/x86/R"
IN_ZIPFILE=android-cts-11_r15-linux_x86-x86.zip
OUT_ZIPFILE=android-cts-x86-11_r15.zip

mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsInputMethodTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsMockInputMethod.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAutoFillServiceTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/arm64/S"
IN_ZIPFILE=android-cts-12_r11-linux_x86-arm.zip
OUT_ZIPFILE=android-cts-arm64-12_r11.zip

mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsInputMethodTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsMockInputMethod.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAutoFillServiceTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/x86/S"
IN_ZIPFILE=android-cts-12_r11-linux_x86-x86.zip
OUT_ZIPFILE=android-cts-x86-12_r11.zip

mkdir -p "$TARGET_DIR"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsInputMethodTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsMockInputMethod.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsAutoFillServiceTestCases.apk"
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases.apk"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

# Below this line, we need to use unzip's -j (junk paths) and -d "$UNZIP_DEST"
# flags. This is to rewrite paths, in order to standardize them, as run_cts.py
# assumes they will be the same across archs for a given release.

TARGET_DIR="$PREFIX/arm64/T"
IN_ZIPFILE=android-cts-13_r7-linux_x86-arm.zip
OUT_ZIPFILE=android-cts-arm64-13_r7.zip
UNZIP_DEST=android-cts/testcases

mkdir -p "$TARGET_DIR"
mkdir -p "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases/arm64/CtsWebkitTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp/arm64/CtsWebViewStartupApp.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases/arm64/CtsWidgetTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases/arm64/CtsAssistTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp/arm64/CtsAssistApp.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService/arm64/CtsAssistService.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsMockInputMethod/arm64/CtsMockInputMethod.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAutoFillServiceTestCases/arm64/CtsAutoFillServiceTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases/arm64/CtsTextTestCases.apk" -d "$UNZIP_DEST"
# host-driven CTS separator for readability
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsHostsideWebViewTests/*"
UNZIP_DISABLE_ZIPBOMB_DETECTION=TRUE unzip "$IN_ZIPFILE" "android-cts/tools/*"
UNZIP_DISABLE_ZIPBOMB_DETECTION=TRUE unzip "$IN_ZIPFILE" "android-cts/jdk/*"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsSecurityTestCases/CtsDeviceInfo.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsPreconditions/arm64/CtsPreconditions.apk" -d "$UNZIP_DEST"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/x86/T"
IN_ZIPFILE=android-cts-13_r7-linux_x86-x86.zip
OUT_ZIPFILE=android-cts-x86-13_r7.zip

mkdir -p "$TARGET_DIR"
mkdir -p "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases/x86_64/CtsWebkitTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp/x86_64/CtsWebViewStartupApp.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases/x86_64/CtsWidgetTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases/x86_64/CtsAssistTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp/x86_64/CtsAssistApp.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService/x86_64/CtsAssistService.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsMockInputMethod/x86_64/CtsMockInputMethod.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAutoFillServiceTestCases/x86_64/CtsAutoFillServiceTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases/x86_64/CtsTextTestCases.apk" -d "$UNZIP_DEST"
# host-driven CTS separator for readability
unzip "$IN_ZIPFILE" "android-cts/testcases/CtsHostsideWebViewTests/*"
UNZIP_DISABLE_ZIPBOMB_DETECTION=TRUE unzip "$IN_ZIPFILE" "android-cts/tools/*"
UNZIP_DISABLE_ZIPBOMB_DETECTION=TRUE unzip "$IN_ZIPFILE" "android-cts/jdk/*"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsSecurityTestCases/CtsDeviceInfo.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsPreconditions/x86_64/CtsPreconditions.apk" -d "$UNZIP_DEST"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/arm64/U"
IN_ZIPFILE=android-cts-14_r3-linux_x86-arm.zip
OUT_ZIPFILE=android-cts-arm64-14_r3.zip
UNZIP_DEST=android-cts/testcases

mkdir -p "$TARGET_DIR"
mkdir -p "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases/arm64/CtsWebkitTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp/arm64/CtsWebViewStartupApp.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases/arm64/CtsWidgetTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases/arm64/CtsAssistTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp/arm64/CtsAssistApp.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService/arm64/CtsAssistService.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsMockInputMethod/arm64/CtsMockInputMethod.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAutoFillServiceTestCases/arm64/CtsAutoFillServiceTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases/arm64/CtsTextTestCases.apk" -d "$UNZIP_DEST"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts

# platform separator for readability

TARGET_DIR="$PREFIX/x86/U"
IN_ZIPFILE=android-cts-14_r3-linux_x86-x86.zip
OUT_ZIPFILE=android-cts-x86-14_r3.zip

mkdir -p "$TARGET_DIR"
mkdir -p "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWebkitTestCases/x86_64/CtsWebkitTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWebViewStartupApp/x86_64/CtsWebViewStartupApp.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsWidgetTestCases/x86_64/CtsWidgetTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistTestCases/x86_64/CtsAssistTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistApp/x86_64/CtsAssistApp.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAssistService/x86_64/CtsAssistService.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsMockInputMethod/x86_64/CtsMockInputMethod.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsAutoFillServiceTestCases/x86_64/CtsAutoFillServiceTestCases.apk" -d "$UNZIP_DEST"
unzip -j "$IN_ZIPFILE" "android-cts/testcases/CtsTextTestCases/x86_64/CtsTextTestCases.apk" -d "$UNZIP_DEST"

zip -rm "$TARGET_DIR/$OUT_ZIPFILE" android-cts
