// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/vi_telex.h"

#include <iterator>

namespace vi_telex {

const char* kId = "vi_telex";
bool kIs102 = false;
const char* kTransforms[] = {
    u8"d\u001d?d",
    u8"\u0111",
    u8"a\u001d?a",
    u8"\u00e2",
    u8"e\u001d?e",
    u8"\u00ea",
    u8"o\u001d?o",
    u8"\u00f4",
    u8"a\u001d?w",
    u8"\u0103",
    u8"o\u001d?w",
    u8"\u01a1",
    u8"u\u001d?w",
    u8"\u01b0",
    u8"w",
    u8"\u01b0",
    u8"D\u001d?[Dd]",
    u8"\u0110",
    u8"A\u001d?[aA]",
    u8"\u00c2",
    u8"E\u001d?[eE]",
    u8"\u00ca",
    u8"O\u001d?[oO]",
    u8"\u00d4",
    u8"A\u001d?[wW]",
    u8"\u0102",
    u8"O\u001d?[wW]",
    u8"\u01a0",
    u8"U\u001d?[wW]",
    u8"\u01af",
    u8"W",
    u8"\u01af",
    u8"\u0111\u001dd",
    u8"dd",
    u8"\u0110\u001dD",
    u8"DD",
    u8"\u00e2\u001da",
    u8"aa",
    u8"\u00ea\u001de",
    u8"ee",
    u8"\u00f4\u001do",
    u8"oo",
    u8"\u00c2\u001dA",
    u8"AA",
    u8"\u00ca\u001dE",
    u8"EE",
    u8"\u00d4\u001dO",
    u8"OO",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?[fF]",
    u8"\\1\u0300\\3",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?[sS]",
    u8"\\1\u0301\\3",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?[rR]",
    u8"\\1\u0309\\3",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?[xX]",
    u8"\\1\u0303\\3",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?[jJ]",
    u8"\\1\u0323\\3",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)[fF]",
    u8"\\1\u0300\\2",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)[sS]",
    u8"\\1\u0301\\2",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)[rR]",
    u8"\\1\u0309\\2",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)[xX]",
    u8"\\1\u0303\\2",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)[jJ]",
    u8"\\1\u0323\\2",
    u8"([\u0300\u0301\u0309\u0303\u0323])([a-yA-Y\u001d]*)([zZ])",
    u8"\\2",
    u8"(\u0300)([a-zA-Z\u001d]*)([fF])",
    u8"\\2\\3",
    u8"(\u0301)([a-zA-Z\u001d]*)([sS])",
    u8"\\2\\3",
    u8"(\u0309)([a-zA-Z\u001d]*)([rR])",
    u8"\\2\\3",
    u8"(\u0303)([a-zA-Z\u001d]*)([xX])",
    u8"\\2\\3",
    u8"(\u0323)([a-zA-Z\u001d]*)([jJ])",
    u8"\\2\\3",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY]+)([\u0300\u0301\u0303\u0309\u0323])"
    u8"\u001d?([aeiouyAEIOUY])\u001d?([a-eg-ik-qtuvyA-EG-IK-QTUVY])",
    u8"\\1\\4\\3\\5",
    u8"([\u0300\u0301\u0303\u0309\u0323])(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z])",
    u8"\\2\\1\\3"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace vi_telex
