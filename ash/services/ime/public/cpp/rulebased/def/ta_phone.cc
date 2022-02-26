// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/ta_phone.h"

#include <iterator>

namespace ta_phone {

const char* kId = "ta_phone";
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
const char* kShift[] = {
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
const char* kAltGr[] = {
    u8"\u0b82",  // BackQuote
    u8"\u0bf3",  // Digit1
    u8"\u0bf4",  // Digit2
    u8"\u0bf5",  // Digit3
    u8"\u0bf6",  // Digit4
    u8"\u0bf7",  // Digit5
    u8"\u0bf8",  // Digit6
    u8"\u0bfa",  // Digit7
    u8"\u0bf0",  // Digit8
    u8"\u0bf1",  // Digit9
    u8"\u0bf2",  // Digit0
    u8"\u0bf9",  // Minus
    u8"\u0be6",  // Equal
    u8"\u0be7",  // KeyQ
    u8"\u0be8",  // KeyW
    u8"\u0be9",  // KeyE
    u8"\u0bea",  // KeyR
    u8"\u0beb",  // KeyT
    u8"\u0bec",  // KeyY
    u8"\u0bed",  // KeyU
    u8"\u0bee",  // KeyI
    u8"\u0bef",  // KeyO
    u8"\u0bd0",  // KeyP
    u8"\u0b83",  // BracketLeft
    u8"\u0b85",  // BracketRight
    u8"\u0b86",  // Backslash
    u8"\u0b87",  // KeyA
    u8"\u0b88",  // KeyS
    u8"\u0b89",  // KeyD
    u8"\u0b8a",  // KeyF
    u8"\u0b8e",  // KeyG
    u8"\u0b8f",  // KeyH
    u8"\u0b90",  // KeyJ
    u8"\u0b92",  // KeyK
    u8"\u0b93",  // KeyL
    u8"\u0b94",  // Semicolon
    u8"\u0b95",  // Quote
    u8"\u0b99",  // KeyZ
    u8"\u0b9a",  // KeyX
    u8"\u0b9c",  // KeyC
    u8"\u0b9e",  // KeyV
    u8"\u0b9f",  // KeyB
    u8"\u0ba3",  // KeyN
    u8"\u0ba4",  // KeyM
    u8"\u0ba8",  // Comma
    u8"\u0ba9",  // Period
    u8"\u0baa",  // Slash
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
    u8"\u0bae",  // BackQuote
    u8"\u0baf",  // Digit1
    u8"\u0bb0",  // Digit2
    u8"\u0bb1",  // Digit3
    u8"\u0bb2",  // Digit4
    u8"\u0bb3",  // Digit5
    u8"\u0bb4",  // Digit6
    u8"\u0bb5",  // Digit7
    u8"\u0bb6",  // Digit8
    u8"\u0bb7",  // Digit9
    u8"\u0bb8",  // Digit0
    u8"\u0bb9",  // Minus
    u8"\u0bbe",  // Equal
    u8"\u0bbf",  // KeyQ
    u8"\u0bc0",  // KeyW
    u8"\u0bc1",  // KeyE
    u8"\u0bc2",  // KeyR
    u8"\u0bc6",  // KeyT
    u8"\u0bc7",  // KeyY
    u8"\u0bc8",  // KeyU
    u8"\u0bca",  // KeyI
    u8"\u0bcb",  // KeyO
    u8"\u0bcc",  // KeyP
    u8"\u0bcd",  // BracketLeft
    u8"\u0bd7",  // BracketRight
    u8"",        // Backslash
    u8"",        // KeyA
    u8"",        // KeyS
    u8"",        // KeyD
    u8"",        // KeyF
    u8"",        // KeyG
    u8"",        // KeyH
    u8"",        // KeyJ
    u8"",        // KeyK
    u8"",        // KeyL
    u8"",        // Semicolon
    u8"",        // Quote
    u8"",        // KeyZ
    u8"",        // KeyX
    u8"",        // KeyC
    u8"",        // KeyV
    u8"",        // KeyB
    u8"",        // KeyN
    u8"",        // KeyM
    u8"",        // Comma
    u8"",        // Period
    u8"",        // Slash
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
const char** kKeyMap[8] = {kNormal,   kShift,         kAltGr, kShiftAltGr,
                           kCapslock, kShiftCapslock, kAltGr, kShiftAltGr};
const char* kTransforms[] = {u8"\u0bcd\u0bb1\u0bcd\u0bb1\u0bcd\u001d?i",
                             u8"\u0bcd\u0bb0\u0bbf",
                             u8"\u0bcd\u0bb1\u0bcd\u001d?\\^i",
                             u8"\u0bcd\u0bb0\u0bbf",
                             u8"\u0bcd\u0bb1\u0bcd\u0bb1\u0bcd\u001d?I",
                             u8"\u0bcd\u0bb0\u0bbf",
                             u8"\u0bcd\u0bb1\u0bcd\u001d?\\^I",
                             u8"\u0bcd\u0bb0\u0bbf",
                             u8"\u0bcd\u0b85\u001d?a",
                             u8"\u0bbe",
                             u8"\u0bbf\u001d?i",
                             u8"\u0bc0",
                             u8"\u0bc6\u001d?e",
                             u8"\u0bc0",
                             u8"\u0bc1\u001d?u",
                             u8"\u0bc2",
                             u8"\u0bca\u001d?o",
                             u8"\u0bc2",
                             u8"\u0bcd\u0b85\u001d?i",
                             u8"\u0bc8",
                             u8"\u0bcd\u0b85\u001d?u",
                             u8"\u0bcc",
                             u8"\u0bca\u001d?u",
                             u8"\u0bcc",
                             u8"\u0bcd\u001d?a",
                             u8"",
                             u8"\u0bcd\u001d?A",
                             u8"\u0bbe",
                             u8"\u0bcd\u001d?i",
                             u8"\u0bbf",
                             u8"\u0bcd\u001d?I",
                             u8"\u0bc0",
                             u8"\u0bcd\u001d?u",
                             u8"\u0bc1",
                             u8"\u0bcd\u001d?U",
                             u8"\u0bc2",
                             u8"\u0bcd\u001d?e",
                             u8"\u0bc6",
                             u8"\u0bcd\u001d?E",
                             u8"\u0bc7",
                             u8"\u0bcd\u001d?o",
                             u8"\u0bca",
                             u8"\u0bcd\u001d?O",
                             u8"\u0bcb",
                             u8"\u0ba9\u0bcd\u001d?ch",
                             u8"\u0b9e\u0bcd\u0b9a\u0bcd",
                             u8"\u0b95\u0bcd\u0b9a\u0bcd\u001d?h",
                             u8"\u0b95\u0bcd\u0bb7\u0bcd",
                             u8"\u0b9a\u0bcd\u0bb0\u0bcd\u001d?i",
                             u8"\u0bb8\u0bcd\u0bb0\u0bc0",
                             u8"\u0b9f\u0bcd\u0b9f\u0bcd\u001d?r",
                             u8"\u0bb1\u0bcd\u0bb1\u0bcd",
                             u8"\u0b85\u001d?a",
                             u8"\u0b86",
                             u8"\u0b87\u001d?i",
                             u8"\u0b88",
                             u8"\u0b8e\u001d?e",
                             u8"\u0b88",
                             u8"\u0b89\u001d?u",
                             u8"\u0b8a",
                             u8"\u0b92\u001d?o",
                             u8"\u0b8a",
                             u8"\u0b85\u001d?i",
                             u8"\u0b90",
                             u8"\u0b85\u001d?u",
                             u8"\u0b94",
                             u8"\u0b92\u001d?u",
                             u8"\u0b94",
                             u8"\u0ba9\u0bcd\u001d?g",
                             u8"\u0b99\u0bcd",
                             u8"ch",
                             u8"\u0b9a\u0bcd",
                             u8"\u0ba9\u0bcd\u001d?j",
                             u8"\u0b9e\u0bcd",
                             u8"\u0b9f\u0bcd\u001d?h",
                             u8"\u0ba4\u0bcd",
                             u8"\u0b9a\u0bcd\u001d?h",
                             u8"\u0bb7\u0bcd",
                             u8"\u0bb8\u0bcd\u001d?h",
                             u8"\u0bb6\u0bcd",
                             u8"\u0bb4\u0bcd\u001d?h",
                             u8"\u0bb4\u0bcd",
                             u8"\u0b95\u0bcd\u001d?S",
                             u8"\u0b95\u0bcd\u0bb7\u0bcd",
                             u8"\u0b9f\u0bcd\u001d?r",
                             u8"\u0bb1\u0bcd\u0bb1\u0bcd",
                             u8"_",
                             u8"\u200b",
                             u8"M",
                             u8"\u0b82",
                             u8"H",
                             u8"\u0b83",
                             u8"a",
                             u8"\u0b85",
                             u8"A",
                             u8"\u0b86",
                             u8"i",
                             u8"\u0b87",
                             u8"I",
                             u8"\u0b88",
                             u8"u",
                             u8"\u0b89",
                             u8"U",
                             u8"\u0b8a",
                             u8"e",
                             u8"\u0b8e",
                             u8"E",
                             u8"\u0b8f",
                             u8"o",
                             u8"\u0b92",
                             u8"O",
                             u8"\u0b93",
                             u8"k",
                             u8"\u0b95\u0bcd",
                             u8"g",
                             u8"\u0b95\u0bcd",
                             u8"q",
                             u8"\u0b95\u0bcd",
                             u8"G",
                             u8"\u0b95\u0bcd",
                             u8"s",
                             u8"\u0b9a\u0bcd",
                             u8"j",
                             u8"\u0b9c\u0bcd",
                             u8"J",
                             u8"\u0b9c\u0bcd",
                             u8"t",
                             u8"\u0b9f\u0bcd",
                             u8"T",
                             u8"\u0b9f\u0bcd",
                             u8"d",
                             u8"\u0b9f\u0bcd",
                             u8"D",
                             u8"\u0b9f\u0bcd",
                             u8"N",
                             u8"\u0ba3\u0bcd",
                             u8"n",
                             u8"\u0ba9\u0bcd",
                             u8"p",
                             u8"\u0baa\u0bcd",
                             u8"b",
                             u8"\u0baa\u0bcd",
                             u8"f",
                             u8"\u0baa\u0bcd",
                             u8"m",
                             u8"\u0bae\u0bcd",
                             u8"y",
                             u8"\u0baf\u0bcd",
                             u8"Y",
                             u8"\u0baf\u0bcd",
                             u8"r",
                             u8"\u0bb0\u0bcd",
                             u8"l",
                             u8"\u0bb2\u0bcd",
                             u8"L",
                             u8"\u0bb3\u0bcd",
                             u8"v",
                             u8"\u0bb5\u0bcd",
                             u8"w",
                             u8"\u0bb5\u0bcd",
                             u8"S",
                             u8"\u0bb8\u0bcd",
                             u8"h",
                             u8"\u0bb9\u0bcd",
                             u8"z",
                             u8"\u0bb4\u0bcd",
                             u8"R",
                             u8"\u0bb1\u0bcd",
                             u8"x",
                             u8"\u0b95\u0bcd\u0bb7\u0bcd",
                             u8"([\u0b95-\u0bb9])\u001d?a",
                             u8"\\1\u0bbe",
                             u8"([\u0b95-\u0bb9])\u001d?i",
                             u8"\\1\u0bc8",
                             u8"([\u0b95-\u0bb9])\u001d?u",
                             u8"\\1\u0bcc",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?a",
                             u8"\\1\u0ba8",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?A",
                             u8"\\1\u0ba8\u0bbe",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?i",
                             u8"\\1\u0ba8\u0bbf",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?I",
                             u8"\\1\u0ba8\u0bc0",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?u",
                             u8"\\1\u0ba8\u0bc1",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?U",
                             u8"\\1\u0ba8\u0bc2",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?e",
                             u8"\\1\u0ba8\u0bc6",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?E",
                             u8"\\1\u0ba8\u0bc7",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?o",
                             u8"\\1\u0ba8\u0bca",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?O",
                             u8"\\1\u0ba8\u0bcb",
                             u8"\u0ba9\u0bcd\u001d?dha",
                             u8"\u0ba8\u0bcd\u0ba4",
                             u8"\u0ba9\u0bcd\u001d?dhA",
                             u8"\u0ba8\u0bcd\u0ba4\u0bbe",
                             u8"\u0ba9\u0bcd\u001d?dhi",
                             u8"\u0ba8\u0bcd\u0ba4\u0bbf",
                             u8"\u0ba9\u0bcd\u001d?dhI",
                             u8"\u0ba8\u0bcd\u0ba4\u0bc0",
                             u8"\u0ba9\u0bcd\u001d?dhu",
                             u8"\u0ba8\u0bcd\u0ba4\u0bc1",
                             u8"\u0ba9\u0bcd\u001d?dhU",
                             u8"\u0ba8\u0bcd\u0ba4\u0bc2",
                             u8"\u0ba9\u0bcd\u001d?dhe",
                             u8"\u0ba8\u0bcd\u0ba4\u0bc6",
                             u8"\u0ba9\u0bcd\u001d?dhE",
                             u8"\u0ba8\u0bcd\u0ba4\u0bc7",
                             u8"\u0ba9\u0bcd\u001d?dho",
                             u8"\u0ba8\u0bcd\u0ba4\u0bca",
                             u8"\u0ba9\u0bcd\u001d?dhO",
                             u8"\u0ba8\u0bcd\u0ba4\u0bcb",
                             u8"([\u0b80-\u0bff])\u0ba9\u0bcd\u001d?g",
                             u8"\\1\u0b99\u0bcd\u0b95\u0bcd",
                             u8"([\u0b80-\u0bff])\u0ba9\u0bcd\u001d?j",
                             u8"\\1\u0b9e\u0bcd\u0b9a\u0bcd",
                             u8"([^\u0b80-\u0bff]|^)\u0ba9\u0bcd\u001d?y",
                             u8"\\1\u0b9e\u0bcd",
                             u8"\u0ba9\u0bcd\u001d?[dt]",
                             u8"\u0ba3\u0bcd\u0b9f\u0bcd",
                             u8"\u0ba3\u0bcd\u0b9f\u0bcd\u001d?h",
                             u8"\u0ba8\u0bcd\u0ba4\u0bcd",
                             u8"\u0ba9\u0bcd\u001d?dh",
                             u8"\u0ba8\u0bcd",
                             u8"\u0ba9\u0bcd\u001d?tr",
                             u8"\u0ba9\u0bcd\u0b9f\u0bcd\u0bb0\u0bcd",
                             u8"\u0ba3\u0bcd\u0b9f\u0bcd\u001d?r",
                             u8"\u0ba9\u0bcd\u0bb1\u0bcd"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = "t|dh|d";

}  // namespace ta_phone
