// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/si.h"

#include <iterator>

namespace si {

const char* kId = "si";
bool kIs102 = false;
const char* kNormal[] = {
    u8"\u0dca\u200d\u0dbb",  // BackQuote
    u8"1",                   // Digit1
    u8"2",                   // Digit2
    u8"3",                   // Digit3
    u8"4",                   // Digit4
    u8"5",                   // Digit5
    u8"6",                   // Digit6
    u8"7",                   // Digit7
    u8"8",                   // Digit8
    u8"9",                   // Digit9
    u8"0",                   // Digit0
    u8"-",                   // Minus
    u8"=",                   // Equal
    u8"\u0dd4",              // KeyQ
    u8"\u0d85",              // KeyW
    u8"\u0dd0",              // KeyE
    u8"\u0dbb",              // KeyR
    u8"\u0d91",              // KeyT
    u8"\u0dc4",              // KeyY
    u8"\u0db8",              // KeyU
    u8"\u0dc3",              // KeyI
    u8"\u0daf",              // KeyO
    u8"\u0da0",              // KeyP
    u8"\u0da4",              // BracketLeft
    u8";",                   // BracketRight
    u8"\u200d\u0dca",        // Backslash
    u8"\u0dca",              // KeyA
    u8"\u0dd2",              // KeyS
    u8"\u0dcf",              // KeyD
    u8"\u0dd9",              // KeyF
    u8"\u0da7",              // KeyG
    u8"\u0dba",              // KeyH
    u8"\u0dc0",              // KeyJ
    u8"\u0db1",              // KeyK
    u8"\u0d9a",              // KeyL
    u8"\u0dad",              // Semicolon
    u8".",                   // Quote
    u8"'",                   // KeyZ
    u8"\u0d82",              // KeyX
    u8"\u0da2",              // KeyC
    u8"\u0da9",              // KeyV
    u8"\u0d89",              // KeyB
    u8"\u0db6",              // KeyN
    u8"\u0db4",              // KeyM
    u8"\u0dbd",              // Comma
    u8"\u0d9c",              // Period
    u8"?",                   // Slash
    u8"\u0020",              // Space
};
const char* kShift[] = {
    u8"\u0dbb\u0dca\u200d",  // BackQuote
    u8"!",                   // Digit1
    u8"@",                   // Digit2
    u8"#",                   // Digit3
    u8"$",                   // Digit4
    u8"%",                   // Digit5
    u8"^",                   // Digit6
    u8"&",                   // Digit7
    u8"*",                   // Digit8
    u8"(",                   // Digit9
    u8")",                   // Digit0
    u8"_",                   // Minus
    u8"+",                   // Equal
    u8"\u0dd6",              // KeyQ
    u8"\u0d8b",              // KeyW
    u8"\u0dd1",              // KeyE
    u8"\u0d8d",              // KeyR
    u8"\u0d94",              // KeyT
    u8"\u0dc1",              // KeyY
    u8"\u0db9",              // KeyU
    u8"\u0dc2",              // KeyI
    u8"\u0db0",              // KeyO
    u8"\u0da1",              // KeyP
    u8"\u0da5",              // BracketLeft
    u8":",                   // BracketRight
    u8"\u0dca\u200d",        // Backslash
    u8"\u0ddf",              // KeyA
    u8"\u0dd3",              // KeyS
    u8"\u0dd8",              // KeyD
    u8"\u0dc6",              // KeyF
    u8"\u0da8",              // KeyG
    u8"\u0dca\u200d\u0dba",  // KeyH
    u8"",                    // KeyJ
    u8"\u0dab",              // KeyK
    u8"\u0d9b",              // KeyL
    u8"\u0dae",              // Semicolon
    u8",",                   // Quote
    u8"\"",                  // KeyZ
    u8"\u0d9e",              // KeyX
    u8"\u0da3",              // KeyC
    u8"\u0daa",              // KeyV
    u8"\u0d8a",              // KeyB
    u8"\u0db7",              // KeyN
    u8"\u0db5",              // KeyM
    u8"\u0dc5",              // Comma
    u8"\u0d9d",              // Period
    u8"/",                   // Slash
    u8"\u0020",              // Space
};
const char* kAltGr[] = {
    u8"",        // BackQuote
    u8"",        // Digit1
    u8"",        // Digit2
    u8"",        // Digit3
    u8"",        // Digit4
    u8"",        // Digit5
    u8"",        // Digit6
    u8"",        // Digit7
    u8"",        // Digit8
    u8"",        // Digit9
    u8"",        // Digit0
    u8"",        // Minus
    u8"",        // Equal
    u8"",        // KeyQ
    u8"",        // KeyW
    u8"",        // KeyE
    u8"",        // KeyR
    u8"",        // KeyT
    u8"",        // KeyY
    u8"",        // KeyU
    u8"",        // KeyI
    u8"\u0db3",  // KeyO
    u8"",        // KeyP
    u8"",        // BracketLeft
    u8"",        // BracketRight
    u8"",        // Backslash
    u8"\u0df3",  // KeyA
    u8"",        // KeyS
    u8"",        // KeyD
    u8"",        // KeyF
    u8"",        // KeyG
    u8"",        // KeyH
    u8"",        // KeyJ
    u8"",        // KeyK
    u8"",        // KeyL
    u8"",        // Semicolon
    u8"\u0df4",  // Quote
    u8"\u0d80",  // KeyZ
    u8"\u0d83",  // KeyX
    u8"\u0da6",  // KeyC
    u8"\u0dac",  // KeyV
    u8"",        // KeyB
    u8"",        // KeyN
    u8"",        // KeyM
    u8"\u0d8f",  // Comma
    u8"\u0d9f",  // Period
    u8"",        // Slash
    u8"\u0020",  // Space
};
const char* kCapslock[] = {
    u8"\u0dbb\u0dca\u200d",  // BackQuote
    u8"!",                   // Digit1
    u8"@",                   // Digit2
    u8"#",                   // Digit3
    u8"$",                   // Digit4
    u8"%",                   // Digit5
    u8"^",                   // Digit6
    u8"&",                   // Digit7
    u8"*",                   // Digit8
    u8"(",                   // Digit9
    u8")",                   // Digit0
    u8"_",                   // Minus
    u8"+",                   // Equal
    u8"\u0dd6",              // KeyQ
    u8"\u0d8b",              // KeyW
    u8"\u0dd1",              // KeyE
    u8"\u0d8d",              // KeyR
    u8"\u0d94",              // KeyT
    u8"\u0dc1",              // KeyY
    u8"\u0db9",              // KeyU
    u8"\u0dc2",              // KeyI
    u8"\u0db0",              // KeyO
    u8"\u0da1",              // KeyP
    u8"\u0da5",              // BracketLeft
    u8":",                   // BracketRight
    u8"\u0dca\u200d",        // Backslash
    u8"\u0ddf",              // KeyA
    u8"\u0dd3",              // KeyS
    u8"\u0dd8",              // KeyD
    u8"\u0dc6",              // KeyF
    u8"\u0da8",              // KeyG
    u8"\u0dca\u200d\u0dba",  // KeyH
    u8"",                    // KeyJ
    u8"\u0dab",              // KeyK
    u8"\u0d9b",              // KeyL
    u8"\u0dae",              // Semicolon
    u8",",                   // Quote
    u8"\"",                  // KeyZ
    u8"\u0d9e",              // KeyX
    u8"\u0da3",              // KeyC
    u8"\u0daa",              // KeyV
    u8"\u0d8a",              // KeyB
    u8"\u0db7",              // KeyN
    u8"\u0db5",              // KeyM
    u8"\u0dc5",              // Comma
    u8"\u0d9d",              // Period
    u8"/",                   // Slash
    u8"\u0020",              // Space
};
const char* kShiftAltGr[] = {
    u8"",        // BackQuote
    u8"",        // Digit1
    u8"",        // Digit2
    u8"",        // Digit3
    u8"",        // Digit4
    u8"",        // Digit5
    u8"",        // Digit6
    u8"",        // Digit7
    u8"",        // Digit8
    u8"",        // Digit9
    u8"",        // Digit0
    u8"",        // Minus
    u8"",        // Equal
    u8"",        // KeyQ
    u8"",        // KeyW
    u8"",        // KeyE
    u8"",        // KeyR
    u8"",        // KeyT
    u8"",        // KeyY
    u8"",        // KeyU
    u8"",        // KeyI
    u8"\u0db3",  // KeyO
    u8"",        // KeyP
    u8"",        // BracketLeft
    u8"",        // BracketRight
    u8"",        // Backslash
    u8"\u0df3",  // KeyA
    u8"",        // KeyS
    u8"",        // KeyD
    u8"",        // KeyF
    u8"",        // KeyG
    u8"",        // KeyH
    u8"",        // KeyJ
    u8"",        // KeyK
    u8"",        // KeyL
    u8"",        // Semicolon
    u8"\u0df4",  // Quote
    u8"\u0d80",  // KeyZ
    u8"\u0d83",  // KeyX
    u8"\u0da6",  // KeyC
    u8"\u0dac",  // KeyV
    u8"",        // KeyB
    u8"",        // KeyN
    u8"",        // KeyM
    u8"\u0d8f",  // Comma
    u8"\u0d9f",  // Period
    u8"",        // Slash
    u8"\u0020",  // Space
};
const char* kAltgrCapslock[] = {
    u8"",        // BackQuote
    u8"",        // Digit1
    u8"",        // Digit2
    u8"",        // Digit3
    u8"",        // Digit4
    u8"",        // Digit5
    u8"",        // Digit6
    u8"",        // Digit7
    u8"",        // Digit8
    u8"",        // Digit9
    u8"",        // Digit0
    u8"",        // Minus
    u8"",        // Equal
    u8"",        // KeyQ
    u8"",        // KeyW
    u8"",        // KeyE
    u8"",        // KeyR
    u8"",        // KeyT
    u8"",        // KeyY
    u8"",        // KeyU
    u8"",        // KeyI
    u8"\u0db3",  // KeyO
    u8"",        // KeyP
    u8"",        // BracketLeft
    u8"",        // BracketRight
    u8"",        // Backslash
    u8"\u0df3",  // KeyA
    u8"",        // KeyS
    u8"",        // KeyD
    u8"",        // KeyF
    u8"",        // KeyG
    u8"",        // KeyH
    u8"",        // KeyJ
    u8"",        // KeyK
    u8"",        // KeyL
    u8"",        // Semicolon
    u8"\u0df4",  // Quote
    u8"\u0d80",  // KeyZ
    u8"\u0d83",  // KeyX
    u8"\u0da6",  // KeyC
    u8"\u0dac",  // KeyV
    u8"",        // KeyB
    u8"",        // KeyN
    u8"",        // KeyM
    u8"\u0d8f",  // Comma
    u8"\u0d9f",  // Period
    u8"",        // Slash
    u8"\u0020",  // Space
};
const char* kShiftCapslock[] = {
    u8"\u0dca\u200d\u0dbb",  // BackQuote
    u8"1",                   // Digit1
    u8"2",                   // Digit2
    u8"3",                   // Digit3
    u8"4",                   // Digit4
    u8"5",                   // Digit5
    u8"6",                   // Digit6
    u8"7",                   // Digit7
    u8"8",                   // Digit8
    u8"9",                   // Digit9
    u8"0",                   // Digit0
    u8"-",                   // Minus
    u8"=",                   // Equal
    u8"\u0dd4",              // KeyQ
    u8"\u0d85",              // KeyW
    u8"\u0dd0",              // KeyE
    u8"\u0dbb",              // KeyR
    u8"\u0d91",              // KeyT
    u8"\u0dc4",              // KeyY
    u8"\u0db8",              // KeyU
    u8"\u0dc3",              // KeyI
    u8"\u0daf",              // KeyO
    u8"\u0da0",              // KeyP
    u8"\u0da4",              // BracketLeft
    u8";",                   // BracketRight
    u8"\u200d\u0dca",        // Backslash
    u8"\u0dca",              // KeyA
    u8"\u0dd2",              // KeyS
    u8"\u0dcf",              // KeyD
    u8"\u0dd9",              // KeyF
    u8"\u0da7",              // KeyG
    u8"\u0dba",              // KeyH
    u8"\u0dc0",              // KeyJ
    u8"\u0db1",              // KeyK
    u8"\u0d9a",              // KeyL
    u8"\u0dad",              // Semicolon
    u8".",                   // Quote
    u8"'",                   // KeyZ
    u8"\u0d82",              // KeyX
    u8"\u0da2",              // KeyC
    u8"\u0da9",              // KeyV
    u8"\u0d89",              // KeyB
    u8"\u0db6",              // KeyN
    u8"\u0db4",              // KeyM
    u8"\u0dbd",              // Comma
    u8"\u0d9c",              // Period
    u8"?",                   // Slash
    u8"\u0020",              // Space
};
const char* kShiftAltGrCapslock[] = {
    u8"",        // BackQuote
    u8"",        // Digit1
    u8"",        // Digit2
    u8"",        // Digit3
    u8"",        // Digit4
    u8"",        // Digit5
    u8"",        // Digit6
    u8"",        // Digit7
    u8"",        // Digit8
    u8"",        // Digit9
    u8"",        // Digit0
    u8"",        // Minus
    u8"",        // Equal
    u8"",        // KeyQ
    u8"",        // KeyW
    u8"",        // KeyE
    u8"",        // KeyR
    u8"",        // KeyT
    u8"",        // KeyY
    u8"",        // KeyU
    u8"",        // KeyI
    u8"\u0db3",  // KeyO
    u8"",        // KeyP
    u8"",        // BracketLeft
    u8"",        // BracketRight
    u8"",        // Backslash
    u8"\u0df3",  // KeyA
    u8"",        // KeyS
    u8"",        // KeyD
    u8"",        // KeyF
    u8"",        // KeyG
    u8"",        // KeyH
    u8"",        // KeyJ
    u8"",        // KeyK
    u8"",        // KeyL
    u8"",        // Semicolon
    u8"\u0df4",  // Quote
    u8"\u0d80",  // KeyZ
    u8"\u0d83",  // KeyX
    u8"\u0da6",  // KeyC
    u8"\u0dac",  // KeyV
    u8"",        // KeyB
    u8"",        // KeyN
    u8"",        // KeyM
    u8"\u0d8f",  // Comma
    u8"\u0d9f",  // Period
    u8"",        // Slash
    u8"\u0020",  // Space
};
const char** kKeyMap[8] = {
    kNormal,   kShift,         kAltGr,         kShiftAltGr,
    kCapslock, kShiftCapslock, kAltgrCapslock, kShiftAltGrCapslock};
const char* kTransforms[] = {u8"\u0d85\u0dcf",
                             u8"\u0d86",
                             u8"\u0d85\u0dd0",
                             u8"\u0d87",
                             u8"\u0d85\u0dd1",
                             u8"\u0d88",
                             u8"\u0d8b\u0ddf",
                             u8"\u0d8c",
                             u8"\u0d8d\u0dd8",
                             u8"\u0d8e",
                             u8"\u0d91\u0dca",
                             u8"\u0d92",
                             u8"\u0dd9\u0d91",
                             u8"\u0d93",
                             u8"\u0d94\u0dca",
                             u8"\u0d95",
                             u8"\u0d94\u0ddf",
                             u8"\u0d96",
                             u8"([\u0d9a-\u0dc6])\u0dd8\u0dd8",
                             u8"\\1\u0df2",
                             u8"\u0dd9([\u0d9a-\u0dc6])",
                             u8"\\1\u0dd9",
                             u8"([\u0d9a-\u0dc6])\u0dd9\u001d\u0dca",
                             u8"\\1\u0dda",
                             u8"\u0dd9\u0dd9([\u0d9a-\u0dc6])",
                             u8"\\1\u0ddb",
                             u8"([\u0d9a-\u0dc6])\u0dd9\u001d\u0dcf",
                             u8"\\1\u0ddc",
                             u8"([\u0d9a-\u0dc6])\u0ddc\u001d\u0dca",
                             u8"\\1\u0ddd",
                             u8"([\u0d9a-\u0dc6])\u0dd9\u001d\u0ddf",
                             u8"\\1\u0dde",
                             u8"([\u0d9a-\u0dc6])(\u0dd9)\u001d((\u0dca\u200d["
                             u8"\u0dba\u0dbb])|(\u0dbb\u0dca\u200d))",
                             u8"\\1\\3\\2",
                             u8"([\u0d9a-\u0dc6](\u0dca\u200d[\u0dba\u0dbb])|("
                             u8"\u0dbb\u0dca\u200d))\u0dd9\u001d\u0dca",
                             u8"\\1\u0dda",
                             u8"([\u0d9a-\u0dc6](\u0dca\u200d[\u0dba\u0dbb])|("
                             u8"\u0dbb\u0dca\u200d))\u0dd9\u001d\u0dcf",
                             u8"\\1\u0ddc",
                             u8"([\u0d9a-\u0dc6](\u0dca\u200d[\u0dba\u0dbb])|("
                             u8"\u0dbb\u0dca\u200d))\u0ddc\u001d\u0dca",
                             u8"\\1\u0ddd"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace si
