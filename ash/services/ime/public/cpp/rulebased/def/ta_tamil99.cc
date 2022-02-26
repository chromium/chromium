// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/ta_tamil99.h"

#include <iterator>

namespace ta_tamil99 {

const char* kId = "ta_tamil99";
bool kIs102 = false;
const char* kNormal[] = {
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
    u8"\u0b86",  // KeyQ
    u8"\u0b88",  // KeyW
    u8"\u0b8a",  // KeyE
    u8"\u0b90",  // KeyR
    u8"\u0b8f",  // KeyT
    u8"\u0bb3",  // KeyY
    u8"\u0bb1",  // KeyU
    u8"\u0ba9",  // KeyI
    u8"\u0b9f",  // KeyO
    u8"\u0ba3",  // KeyP
    u8"\u0b9a",  // BracketLeft
    u8"\u0b9e",  // BracketRight
    u8"\\",      // Backslash
    u8"\u0b85",  // KeyA
    u8"\u0b87",  // KeyS
    u8"\u0b89",  // KeyD
    u8"\u0bcd",  // KeyF
    u8"\u0b8e",  // KeyG
    u8"\u0b95",  // KeyH
    u8"\u0baa",  // KeyJ
    u8"\u0bae",  // KeyK
    u8"\u0ba4",  // KeyL
    u8"\u0ba8",  // Semicolon
    u8"\u0baf",  // Quote
    u8"\u0b94",  // KeyZ
    u8"\u0b93",  // KeyX
    u8"\u0b92",  // KeyC
    u8"\u0bb5",  // KeyV
    u8"\u0b99",  // KeyB
    u8"\u0bb2",  // KeyN
    u8"\u0bb0",  // KeyM
    u8",",       // Comma
    u8".",       // Period
    u8"\u0bb4",  // Slash
    u8"\u0020",  // Space
};
const char* kShift[] = {
    u8"~",                         // BackQuote
    u8"!",                         // Digit1
    u8"@",                         // Digit2
    u8"#",                         // Digit3
    u8"$",                         // Digit4
    u8"%",                         // Digit5
    u8"^",                         // Digit6
    u8"&",                         // Digit7
    u8"*",                         // Digit8
    u8"(",                         // Digit9
    u8")",                         // Digit0
    u8"_",                         // Minus
    u8"+",                         // Equal
    u8"\u0bb8",                    // KeyQ
    u8"\u0bb7",                    // KeyW
    u8"\u0b9c",                    // KeyE
    u8"\u0bb9",                    // KeyR
    u8"\u0bb8\u0bcd\u0bb0\u0bc0",  // KeyT
    u8"\u0b95\u0bcd\u0bb7",        // KeyY
    u8"",                          // KeyU
    u8"",                          // KeyI
    u8"[",                         // KeyO
    u8"]",                         // KeyP
    u8"{",                         // BracketLeft
    u8"}",                         // BracketRight
    u8"|",                         // Backslash
    u8"\u0bf9",                    // KeyA
    u8"\u0bfa",                    // KeyS
    u8"\u0bf8",                    // KeyD
    u8"\u0b83",                    // KeyF
    u8"",                          // KeyG
    u8"",                          // KeyH
    u8"",                          // KeyJ
    u8"\"",                        // KeyK
    u8":",                         // KeyL
    u8";",                         // Semicolon
    u8"'",                         // Quote
    u8"\u0bf3",                    // KeyZ
    u8"\u0bf4",                    // KeyX
    u8"\u0bf5",                    // KeyC
    u8"\u0bf6",                    // KeyV
    u8"\u0bf7",                    // KeyB
    u8"",                          // KeyN
    u8"/",                         // KeyM
    u8"<",                         // Comma
    u8">",                         // Period
    u8"?",                         // Slash
    u8"\u0020",                    // Space
};
const char* kAltGr[] = {
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
    u8"\u0b86",  // KeyQ
    u8"\u0b88",  // KeyW
    u8"\u0b8a",  // KeyE
    u8"\u0b90",  // KeyR
    u8"\u0b8f",  // KeyT
    u8"\u0bb3",  // KeyY
    u8"\u0bb1",  // KeyU
    u8"\u0ba9",  // KeyI
    u8"\u0b9f",  // KeyO
    u8"\u0ba3",  // KeyP
    u8"\u0b9a",  // BracketLeft
    u8"\u0b9e",  // BracketRight
    u8"\\",      // Backslash
    u8"\u0b85",  // KeyA
    u8"\u0b87",  // KeyS
    u8"\u0b89",  // KeyD
    u8"\u0bcd",  // KeyF
    u8"\u0b8e",  // KeyG
    u8"\u0b95",  // KeyH
    u8"\u0baa",  // KeyJ
    u8"\u0bae",  // KeyK
    u8"\u0ba4",  // KeyL
    u8"\u0ba8",  // Semicolon
    u8"\u0baf",  // Quote
    u8"\u0b94",  // KeyZ
    u8"\u0b93",  // KeyX
    u8"\u0b92",  // KeyC
    u8"\u0bb5",  // KeyV
    u8"\u0b99",  // KeyB
    u8"\u0bb2",  // KeyN
    u8"\u0bb0",  // KeyM
    u8",",       // Comma
    u8".",       // Period
    u8"\u0bb4",  // Slash
    u8"\u0020",  // Space
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
    u8"[",       // BracketLeft
    u8"]",       // BracketRight
    u8"\\",      // Backslash
    u8"A",       // KeyA
    u8"S",       // KeyS
    u8"D",       // KeyD
    u8"F",       // KeyF
    u8"G",       // KeyG
    u8"H",       // KeyH
    u8"J",       // KeyJ
    u8"K",       // KeyK
    u8"L",       // KeyL
    u8";",       // Semicolon
    u8"'",       // Quote
    u8"Z",       // KeyZ
    u8"X",       // KeyX
    u8"C",       // KeyC
    u8"V",       // KeyV
    u8"B",       // KeyB
    u8"N",       // KeyN
    u8"M",       // KeyM
    u8",",       // Comma
    u8".",       // Period
    u8"/",       // Slash
    u8"\u0020",  // Space
};
const char* kShiftAltGr[] = {
    u8"~",                         // BackQuote
    u8"!",                         // Digit1
    u8"@",                         // Digit2
    u8"#",                         // Digit3
    u8"$",                         // Digit4
    u8"%",                         // Digit5
    u8"^",                         // Digit6
    u8"&",                         // Digit7
    u8"*",                         // Digit8
    u8"(",                         // Digit9
    u8")",                         // Digit0
    u8"_",                         // Minus
    u8"+",                         // Equal
    u8"\u0bb8",                    // KeyQ
    u8"\u0bb7",                    // KeyW
    u8"\u0b9c",                    // KeyE
    u8"\u0bb9",                    // KeyR
    u8"\u0bb8\u0bcd\u0bb0\u0bc0",  // KeyT
    u8"\u0b95\u0bcd\u0bb7",        // KeyY
    u8"",                          // KeyU
    u8"",                          // KeyI
    u8"[",                         // KeyO
    u8"]",                         // KeyP
    u8"{",                         // BracketLeft
    u8"}",                         // BracketRight
    u8"|",                         // Backslash
    u8"\u0bf9",                    // KeyA
    u8"\u0bfa",                    // KeyS
    u8"\u0bf8",                    // KeyD
    u8"\u0b83",                    // KeyF
    u8"",                          // KeyG
    u8"",                          // KeyH
    u8"",                          // KeyJ
    u8"\"",                        // KeyK
    u8":",                         // KeyL
    u8";",                         // Semicolon
    u8"'",                         // Quote
    u8"\u0bf3",                    // KeyZ
    u8"\u0bf4",                    // KeyX
    u8"\u0bf5",                    // KeyC
    u8"\u0bf6",                    // KeyV
    u8"\u0bf7",                    // KeyB
    u8"",                          // KeyN
    u8"/",                         // KeyM
    u8"<",                         // Comma
    u8">",                         // Period
    u8"?",                         // Slash
    u8"\u0020",                    // Space
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
    u8"[",       // BracketLeft
    u8"]",       // BracketRight
    u8"\\",      // Backslash
    u8"A",       // KeyA
    u8"S",       // KeyS
    u8"D",       // KeyD
    u8"F",       // KeyF
    u8"G",       // KeyG
    u8"H",       // KeyH
    u8"J",       // KeyJ
    u8"K",       // KeyK
    u8"L",       // KeyL
    u8";",       // Semicolon
    u8"'",       // Quote
    u8"Z",       // KeyZ
    u8"X",       // KeyX
    u8"C",       // KeyC
    u8"V",       // KeyV
    u8"B",       // KeyB
    u8"N",       // KeyN
    u8"M",       // KeyM
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
    u8"{",       // BracketLeft
    u8"}",       // BracketRight
    u8"|",       // Backslash
    u8"a",       // KeyA
    u8"s",       // KeyS
    u8"d",       // KeyD
    u8"f",       // KeyF
    u8"g",       // KeyG
    u8"h",       // KeyH
    u8"j",       // KeyJ
    u8"k",       // KeyK
    u8"l",       // KeyL
    u8":",       // Semicolon
    u8"\"",      // Quote
    u8"z",       // KeyZ
    u8"x",       // KeyX
    u8"c",       // KeyC
    u8"v",       // KeyV
    u8"b",       // KeyB
    u8"n",       // KeyN
    u8"m",       // KeyM
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
    u8"{",       // BracketLeft
    u8"}",       // BracketRight
    u8"|",       // Backslash
    u8"a",       // KeyA
    u8"s",       // KeyS
    u8"d",       // KeyD
    u8"f",       // KeyF
    u8"g",       // KeyG
    u8"h",       // KeyH
    u8"j",       // KeyJ
    u8"k",       // KeyK
    u8"l",       // KeyL
    u8":",       // Semicolon
    u8"\"",      // Quote
    u8"z",       // KeyZ
    u8"x",       // KeyX
    u8"c",       // KeyC
    u8"v",       // KeyV
    u8"b",       // KeyB
    u8"n",       // KeyN
    u8"m",       // KeyM
    u8"<",       // Comma
    u8">",       // Period
    u8"?",       // Slash
    u8"\u0020",  // Space
};
const char** kKeyMap[8] = {
    kNormal,   kShift,         kAltGr,         kShiftAltGr,
    kCapslock, kShiftCapslock, kAltgrCapslock, kShiftAltGrCapslock};
const char* kTransforms[] = {u8"([\u0b95-\u0bb9])\u0b85", u8"\\1\u200d",
                             u8"([\u0b95-\u0bb9])\u0b86", u8"\\1\u0bbe",
                             u8"([\u0b95-\u0bb9])\u0b87", u8"\\1\u0bbf",
                             u8"([\u0b95-\u0bb9])\u0b88", u8"\\1\u0bc0",
                             u8"([\u0b95-\u0bb9])\u0b89", u8"\\1\u0bc1",
                             u8"([\u0b95-\u0bb9])\u0b8a", u8"\\1\u0bc2",
                             u8"([\u0b95-\u0bb9])\u0b8e", u8"\\1\u0bc6",
                             u8"([\u0b95-\u0bb9])\u0b8f", u8"\\1\u0bc7",
                             u8"([\u0b95-\u0bb9])\u0b90", u8"\\1\u0bc8",
                             u8"([\u0b95-\u0bb9])\u0b92", u8"\\1\u0bca",
                             u8"([\u0b95-\u0bb9])\u0b93", u8"\\1\u0bcb",
                             u8"([\u0b95-\u0bb9])\u0b94", u8"\\1\u0bcc"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace ta_tamil99
