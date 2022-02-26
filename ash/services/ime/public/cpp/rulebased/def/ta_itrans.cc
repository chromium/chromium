// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/ta_itrans.h"

#include <iterator>

namespace ta_itrans {

const char* kId = "ta_itrans";
bool kIs102 = false;
const char* kTransforms[] = {u8"k",    u8"\u0b95\u0bcd",
                             u8"g",    u8"\u0b95\u0bcd",
                             u8"~N",   u8"\u0b99\u0bcd",
                             u8"N\\^", u8"\u0b99\u0bcd",
                             u8"ch",   u8"\u0b9a\u0bcd",
                             u8"j",    u8"\u0b9c\u0bcd",
                             u8"~n",   u8"\u0b9e\u0bcd",
                             u8"JN",   u8"\u0b9e\u0bcd",
                             u8"T",    u8"\u0b9f\u0bcd",
                             u8"Th",   u8"\u0b9f\u0bcd",
                             u8"N",    u8"\u0ba3\u0bcd",
                             u8"t",    u8"\u0ba4\u0bcd",
                             u8"th",   u8"\u0ba4\u0bcd",
                             u8"n",    u8"\u0ba8\u0bcd",
                             u8"\\^n", u8"\u0ba9\u0bcd",
                             u8"nh",   u8"\u0ba9",
                             u8"p",    u8"\u0baa\u0bcd",
                             u8"b",    u8"\u0baa\u0bcd",
                             u8"m",    u8"\u0bae\u0bcd",
                             u8"y",    u8"\u0baf\u0bcd",
                             u8"r",    u8"\u0bb0\u0bcd",
                             u8"R",    u8"\u0bb1\u0bcd",
                             u8"rh",   u8"\u0bb1",
                             u8"l",    u8"\u0bb2\u0bcd",
                             u8"L",    u8"\u0bb3\u0bcd",
                             u8"ld",   u8"\u0bb3\u0bcd",
                             u8"J",    u8"\u0bb4\u0bcd",
                             u8"z",    u8"\u0bb4\u0bcd",
                             u8"v",    u8"\u0bb5\u0bcd",
                             u8"w",    u8"\u0bb5\u0bcd",
                             u8"Sh",   u8"\u0bb7\u0bcd",
                             u8"shh",  u8"\u0bb7",
                             u8"s",    u8"\u0bb8\u0bcd",
                             u8"h",    u8"\u0bb9\u0bcd",
                             u8"GY",   u8"\u0b9c\u0bcd\u0b9e",
                             u8"dny",  u8"\u0b9c\u0bcd\u0b9e",
                             u8"x",    u8"\u0b95\u0bcd\u0bb7\u0bcd",
                             u8"ksh",  u8"\u0b95\u0bcd\u0bb7\u0bcd",
                             u8"a",    u8"\u0b85",
                             u8"aa",   u8"\u0b86",
                             u8"A",    u8"\u0b86",
                             u8"i",    u8"\u0b87",
                             u8"ii",   u8"\u0b88",
                             u8"I",    u8"\u0b88",
                             u8"u",    u8"\u0b89",
                             u8"uu",   u8"\u0b8a",
                             u8"U",    u8"\u0b8a",
                             u8"e",    u8"\u0b8e",
                             u8"E",    u8"\u0b8f",
                             u8"ee",   u8"\u0b8f",
                             u8"ai",   u8"\u0b90",
                             u8"o",    u8"\u0b92",
                             u8"O",    u8"\u0b93",
                             u8"oo",   u8"\u0b93",
                             u8"au",   u8"\u0b94",
                             u8"\\.n", u8"\u0b82",
                             u8"M",    u8"\u0b82",
                             u8"q",    u8"\u0b83",
                             u8"H",    u8"\u0b83",
                             u8"\\.h", u8"\u0bcd",
                             u8"0",    u8"\u0be6",
                             u8"1",    u8"\u0be7",
                             u8"2",    u8"\u0be8",
                             u8"3",    u8"\u0be9",
                             u8"4",    u8"\u0bea",
                             u8"5",    u8"\u0beb",
                             u8"6",    u8"\u0bec",
                             u8"7",    u8"\u0bed",
                             u8"8",    u8"\u0bee",
                             u8"9",    u8"\u0bef",
                             u8"#",    u8"\u0bcd",
                             u8"\\$",  u8"\u0bb0",
                             u8"\\^",  u8"\u0ba4\u0bcd"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = "[\\^lrshkdnJNtTaeiouAEIOU]|sh|ks|dn";

}  // namespace ta_itrans
