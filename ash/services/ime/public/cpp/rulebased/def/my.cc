// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/my.h"

#include <iterator>

namespace my {

const char* kId = "my";
bool kIs102 = false;
const char* kNormal[] = {
    u8"\u1050",        // BackQuote
    u8"\u1041",        // Digit1
    u8"\u1042",        // Digit2
    u8"\u1043",        // Digit3
    u8"\u1044",        // Digit4
    u8"\u1045",        // Digit5
    u8"\u1046",        // Digit6
    u8"\u1047",        // Digit7
    u8"\u1048",        // Digit8
    u8"\u1049",        // Digit9
    u8"\u1040",        // Digit0
    u8"-",             // Minus
    u8"=",             // Equal
    u8"\u1006",        // KeyQ
    u8"\u1010",        // KeyW
    u8"\u1014",        // KeyE
    u8"\u1019",        // KeyR
    u8"\u1021",        // KeyT
    u8"\u1015",        // KeyY
    u8"\u1000",        // KeyU
    u8"\u1004",        // KeyI
    u8"\u101e",        // KeyO
    u8"\u1005",        // KeyP
    u8"\u101f",        // BracketLeft
    u8"\u1029",        // BracketRight
    u8"\u104f",        // Backslash
    u8"\u200c\u1031",  // KeyA
    u8"\u103b",        // KeyS
    u8"\u102d",        // KeyD
    u8"\u103a",        // KeyF
    u8"\u102b",        // KeyG
    u8"\u1037",        // KeyH
    u8"\u103c",        // KeyJ
    u8"\u102f",        // KeyK
    u8"\u1030",        // KeyL
    u8"\u1038",        // Semicolon
    u8"'",             // Quote
    u8"\u1016",        // KeyZ
    u8"\u1011",        // KeyX
    u8"\u1001",        // KeyC
    u8"\u101c",        // KeyV
    u8"\u1018",        // KeyB
    u8"\u100a",        // KeyN
    u8"\u102c",        // KeyM
    u8",",             // Comma
    u8".",             // Period
    u8"/",             // Slash
    u8"\u0020",        // Space
};
const char* kShift[] = {
    u8"\u100e",  // BackQuote
    u8"\u100d",  // Digit1
    u8"\u1052",  // Digit2
    u8"\u100b",  // Digit3
    u8"\u1053",  // Digit4
    u8"\u1054",  // Digit5
    u8"\u1055",  // Digit6
    u8"\u101b",  // Digit7
    u8"*",       // Digit8
    u8"(",       // Digit9
    u8")",       // Digit0
    u8"_",       // Minus
    u8"+",       // Equal
    u8"\u1008",  // KeyQ
    u8"\u101d",  // KeyW
    u8"\u1023",  // KeyE
    u8"\u104e",  // KeyR
    u8"\u1024",  // KeyT
    u8"\u104c",  // KeyY
    u8"\u1025",  // KeyU
    u8"\u104d",  // KeyI
    u8"\u103f",  // KeyO
    u8"\u100f",  // KeyP
    u8"\u1027",  // BracketLeft
    u8"\u102a",  // BracketRight
    u8"\u1051",  // Backslash
    u8"\u1017",  // KeyA
    u8"\u103e",  // KeyS
    u8"\u102e",  // KeyD
    u8"\u1039",  // KeyF
    u8"\u103d",  // KeyG
    u8"\u1036",  // KeyH
    u8"\u1032",  // KeyJ
    u8"\u1012",  // KeyK
    u8"\u1013",  // KeyL
    u8"\u1002",  // Semicolon
    u8"\"",      // Quote
    u8"\u1007",  // KeyZ
    u8"\u100c",  // KeyX
    u8"\u1003",  // KeyC
    u8"\u1020",  // KeyV
    u8"\u101a",  // KeyB
    u8"\u1009",  // KeyN
    u8"\u1026",  // KeyM
    u8"\u104a",  // Comma
    u8"\u104b",  // Period
    u8"?",       // Slash
    u8"\u0020",  // Space
};
const char* kAltGr[] = {
    u8"\u1050",        // BackQuote
    u8"\u1041",        // Digit1
    u8"\u1042",        // Digit2
    u8"\u1043",        // Digit3
    u8"\u1044",        // Digit4
    u8"\u1045",        // Digit5
    u8"\u1046",        // Digit6
    u8"\u1047",        // Digit7
    u8"\u1048",        // Digit8
    u8"\u1049",        // Digit9
    u8"\u1040",        // Digit0
    u8"-",             // Minus
    u8"=",             // Equal
    u8"\u1006",        // KeyQ
    u8"\u1010",        // KeyW
    u8"\u1014",        // KeyE
    u8"\u1019",        // KeyR
    u8"\u1021",        // KeyT
    u8"\u1015",        // KeyY
    u8"\u1000",        // KeyU
    u8"\u1004",        // KeyI
    u8"\u101e",        // KeyO
    u8"\u1005",        // KeyP
    u8"\u101f",        // BracketLeft
    u8"\u1029",        // BracketRight
    u8"\u104f",        // Backslash
    u8"\u200c\u1031",  // KeyA
    u8"\u103b",        // KeyS
    u8"\u102d",        // KeyD
    u8"\u103a",        // KeyF
    u8"\u102b",        // KeyG
    u8"\u1037",        // KeyH
    u8"\u103c",        // KeyJ
    u8"\u102f",        // KeyK
    u8"\u1030",        // KeyL
    u8"\u1038",        // Semicolon
    u8"'",             // Quote
    u8"\u1016",        // KeyZ
    u8"\u1011",        // KeyX
    u8"\u1001",        // KeyC
    u8"\u101c",        // KeyV
    u8"\u1018",        // KeyB
    u8"\u100a",        // KeyN
    u8"\u102c",        // KeyM
    u8",",             // Comma
    u8".",             // Period
    u8"/",             // Slash
    u8"\u0020",        // Space
};
const char* kCapslock[] = {
    u8"`",       // BackQuote
    u8"1",       // Digit1
    u8"2",       // Digit2
    u8"3",       // Digit3
    u8"4",       // Digit4
    u8"5",       // Digit5
    u8"6",       // Digit6
    u8"7",       // Digit7
    u8"8",       // Digit8
    u8"9",       // Digit9
    u8"0",       // Digit0
    u8"-",       // Minus
    u8"=",       // Equal
    u8"q",       // KeyQ
    u8"w",       // KeyW
    u8"e",       // KeyE
    u8"r",       // KeyR
    u8"t",       // KeyT
    u8"y",       // KeyY
    u8"u",       // KeyU
    u8"i",       // KeyI
    u8"o",       // KeyO
    u8"p",       // KeyP
    u8"[",       // BracketLeft
    u8"]",       // BracketRight
    u8"\\",      // Backslash
    u8"a",       // KeyA
    u8"s",       // KeyS
    u8"d",       // KeyD
    u8"f",       // KeyF
    u8"g",       // KeyG
    u8"h",       // KeyH
    u8"j",       // KeyJ
    u8"k",       // KeyK
    u8"l",       // KeyL
    u8";",       // Semicolon
    u8"'",       // Quote
    u8"z",       // KeyZ
    u8"x",       // KeyX
    u8"c",       // KeyC
    u8"v",       // KeyV
    u8"b",       // KeyB
    u8"n",       // KeyN
    u8"m",       // KeyM
    u8",",       // Comma
    u8".",       // Period
    u8"/",       // Slash
    u8"\u0020",  // Space
};
const char* kShiftAltGr[] = {
    u8"\u100e",  // BackQuote
    u8"\u100d",  // Digit1
    u8"\u1052",  // Digit2
    u8"\u100b",  // Digit3
    u8"\u1053",  // Digit4
    u8"\u1054",  // Digit5
    u8"\u1055",  // Digit6
    u8"\u101b",  // Digit7
    u8"*",       // Digit8
    u8"(",       // Digit9
    u8")",       // Digit0
    u8"_",       // Minus
    u8"+",       // Equal
    u8"\u1008",  // KeyQ
    u8"\u101d",  // KeyW
    u8"\u1023",  // KeyE
    u8"\u104e",  // KeyR
    u8"\u1024",  // KeyT
    u8"\u104c",  // KeyY
    u8"\u1025",  // KeyU
    u8"\u104d",  // KeyI
    u8"\u103f",  // KeyO
    u8"\u100f",  // KeyP
    u8"\u1027",  // BracketLeft
    u8"\u102a",  // BracketRight
    u8"\u1051",  // Backslash
    u8"\u1017",  // KeyA
    u8"\u103e",  // KeyS
    u8"\u102e",  // KeyD
    u8"\u1039",  // KeyF
    u8"\u103d",  // KeyG
    u8"\u1036",  // KeyH
    u8"\u1032",  // KeyJ
    u8"\u1012",  // KeyK
    u8"\u1013",  // KeyL
    u8"\u1002",  // Semicolon
    u8"\"",      // Quote
    u8"\u1007",  // KeyZ
    u8"\u100c",  // KeyX
    u8"\u1003",  // KeyC
    u8"\u1020",  // KeyV
    u8"\u101a",  // KeyB
    u8"\u1009",  // KeyN
    u8"\u1026",  // KeyM
    u8"\u104a",  // Comma
    u8"\u104b",  // Period
    u8"?",       // Slash
    u8"\u0020",  // Space
};
const char* kAltgrCapslock[] = {
    u8"`",       // BackQuote
    u8"1",       // Digit1
    u8"2",       // Digit2
    u8"3",       // Digit3
    u8"4",       // Digit4
    u8"5",       // Digit5
    u8"6",       // Digit6
    u8"7",       // Digit7
    u8"8",       // Digit8
    u8"9",       // Digit9
    u8"0",       // Digit0
    u8"-",       // Minus
    u8"=",       // Equal
    u8"q",       // KeyQ
    u8"w",       // KeyW
    u8"e",       // KeyE
    u8"r",       // KeyR
    u8"t",       // KeyT
    u8"y",       // KeyY
    u8"u",       // KeyU
    u8"i",       // KeyI
    u8"o",       // KeyO
    u8"p",       // KeyP
    u8"[",       // BracketLeft
    u8"]",       // BracketRight
    u8"\\",      // Backslash
    u8"a",       // KeyA
    u8"s",       // KeyS
    u8"d",       // KeyD
    u8"f",       // KeyF
    u8"g",       // KeyG
    u8"h",       // KeyH
    u8"j",       // KeyJ
    u8"k",       // KeyK
    u8"l",       // KeyL
    u8";",       // Semicolon
    u8"'",       // Quote
    u8"z",       // KeyZ
    u8"x",       // KeyX
    u8"c",       // KeyC
    u8"v",       // KeyV
    u8"b",       // KeyB
    u8"n",       // KeyN
    u8"m",       // KeyM
    u8",",       // Comma
    u8".",       // Period
    u8"/",       // Slash
    u8"\u0020",  // Space
};
const char* kShiftCapslock[] = {
    u8"~",       // BackQuote
    u8"!",       // Digit1
    u8"@",       // Digit2
    u8"#",       // Digit3
    u8"$",       // Digit4
    u8"%",       // Digit5
    u8"^",       // Digit6
    u8"&",       // Digit7
    u8"*",       // Digit8
    u8"(",       // Digit9
    u8")",       // Digit0
    u8"_",       // Minus
    u8"+",       // Equal
    u8"Q",       // KeyQ
    u8"W",       // KeyW
    u8"E",       // KeyE
    u8"R",       // KeyR
    u8"T",       // KeyT
    u8"Y",       // KeyY
    u8"U",       // KeyU
    u8"I",       // KeyI
    u8"O",       // KeyO
    u8"P",       // KeyP
    u8"{",       // BracketLeft
    u8"}",       // BracketRight
    u8"|",       // Backslash
    u8"A",       // KeyA
    u8"S",       // KeyS
    u8"D",       // KeyD
    u8"F",       // KeyF
    u8"G",       // KeyG
    u8"H",       // KeyH
    u8"J",       // KeyJ
    u8"K",       // KeyK
    u8"L",       // KeyL
    u8":",       // Semicolon
    u8"\"",      // Quote
    u8"Z",       // KeyZ
    u8"X",       // KeyX
    u8"C",       // KeyC
    u8"V",       // KeyV
    u8"B",       // KeyB
    u8"N",       // KeyN
    u8"M",       // KeyM
    u8"<",       // Comma
    u8">",       // Period
    u8"?",       // Slash
    u8"\u0020",  // Space
};
const char* kShiftAltGrCapslock[] = {
    u8"~",       // BackQuote
    u8"!",       // Digit1
    u8"@",       // Digit2
    u8"#",       // Digit3
    u8"$",       // Digit4
    u8"%",       // Digit5
    u8"^",       // Digit6
    u8"&",       // Digit7
    u8"*",       // Digit8
    u8"(",       // Digit9
    u8")",       // Digit0
    u8"_",       // Minus
    u8"+",       // Equal
    u8"Q",       // KeyQ
    u8"W",       // KeyW
    u8"E",       // KeyE
    u8"R",       // KeyR
    u8"T",       // KeyT
    u8"Y",       // KeyY
    u8"U",       // KeyU
    u8"I",       // KeyI
    u8"O",       // KeyO
    u8"P",       // KeyP
    u8"{",       // BracketLeft
    u8"}",       // BracketRight
    u8"|",       // Backslash
    u8"A",       // KeyA
    u8"S",       // KeyS
    u8"D",       // KeyD
    u8"F",       // KeyF
    u8"G",       // KeyG
    u8"H",       // KeyH
    u8"J",       // KeyJ
    u8"K",       // KeyK
    u8"L",       // KeyL
    u8":",       // Semicolon
    u8"\"",      // Quote
    u8"Z",       // KeyZ
    u8"X",       // KeyX
    u8"C",       // KeyC
    u8"V",       // KeyV
    u8"B",       // KeyB
    u8"N",       // KeyN
    u8"M",       // KeyM
    u8"<",       // Comma
    u8">",       // Period
    u8"?",       // Slash
    u8"\u0020",  // Space
};
const char** kKeyMap[8] = {
    kNormal,   kShift,         kAltGr,         kShiftAltGr,
    kCapslock, kShiftCapslock, kAltgrCapslock, kShiftAltGrCapslock};
const char* kTransforms[] = {
    u8"\u200c\u1031([\u1000-\u102a\u103f\u104e])",
    u8"\\1\u1031",
    u8"([\u103c-\u103e]*\u1031)\u001d\u103b",
    u8"\u103b\\1",
    u8"([\u103b]*)([\u103d-\u103e]*)\u1031\u001d\u103c",
    u8"\\1\u103c\\2\u1031",
    u8"([\u103b\u103c]*)([\u103e]*)\u1031\u001d\u103d",
    u8"\\1\u103d\\2\u1031",
    u8"([\u103b-\u103d]*)\u1031\u001d\u103e",
    u8"\\1\u103e\u1031",
    u8"([\u103c-\u103e]+)\u001d?\u103b",
    u8"\u103b\\1",
    u8"([\u103b]*)([\u103d-\u103e]+)\u001d?\u103c",
    u8"\\1\u103c\\2",
    u8"([\u103b\u103c]*)([\u103e]+)\u001d?\u103d",
    u8"\\1\u103d\\2",
    u8"\u1004\u1031\u001d\u103a",
    u8"\u1004\u103a\u1031",
    u8"\u1004\u103a\u1031\u001d\u1039",
    u8"\u1004\u103a\u1039\u1031",
    u8"\u1004\u103a\u1039\u1031\u001d([\u1000-\u102a\u103f\u104e])",
    u8"\u1004\u103a\u1039\\1\u1031",
    u8"([\u1000-\u102a\u103f\u104e])\u1031\u001d\u1039",
    u8"\\1\u1039\u1031",
    u8"\u1039\u1031\u001d([\u1000-\u1019\u101c\u101e\u1020\u1021])",
    u8"\u1039\\1\u1031"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace my
