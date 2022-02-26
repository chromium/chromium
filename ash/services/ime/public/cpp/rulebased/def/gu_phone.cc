// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/gu_phone.h"

#include <iterator>

namespace gu_phone {

const char* kId = "gu_phone";
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
    u8"\u0af1",              // BackQuote
    u8"\u0ae6",              // Digit1
    u8"\u0ae7",              // Digit2
    u8"\u0ae8",              // Digit3
    u8"\u0ae9",              // Digit4
    u8"\u0aea",              // Digit5
    u8"\u0aeb",              // Digit6
    u8"\u0aec",              // Digit7
    u8"\u0aed",              // Digit8
    u8"\u0aee",              // Digit9
    u8"\u0aef",              // Digit0
    u8"\u0ad0",              // Minus
    u8"\u0a85",              // Equal
    u8"\u0a85\u0a82",        // KeyQ
    u8"\u0a85\u0a83",        // KeyW
    u8"\u0a86",              // KeyE
    u8"\u0a87",              // KeyR
    u8"\u0a88",              // KeyT
    u8"\u0a89",              // KeyY
    u8"\u0a8a",              // KeyU
    u8"\u0a8b",              // KeyI
    u8"\u0ae0",              // KeyO
    u8"\u0a8c",              // KeyP
    u8"\u0ae1",              // BracketLeft
    u8"\u0a8d",              // BracketRight
    u8"\u0a8f",              // Backslash
    u8"\u0a90",              // KeyA
    u8"\u0a91",              // KeyS
    u8"\u0a93",              // KeyD
    u8"\u0a94",              // KeyF
    u8"\u0a95",              // KeyG
    u8"\u0a95\u0acd\u0ab7",  // KeyH
    u8"\u0a96",              // KeyJ
    u8"\u0a97",              // KeyK
    u8"\u0a98",              // KeyL
    u8"\u0a99",              // Semicolon
    u8"\u0a9a",              // Quote
    u8"\u0a9b",              // KeyZ
    u8"\u0a9c",              // KeyX
    u8"\u0a9c\u0acd\u0a9e",  // KeyC
    u8"\u0a9d",              // KeyV
    u8"\u0a9e",              // KeyB
    u8"\u0a9f",              // KeyN
    u8"\u0aa0",              // KeyM
    u8"\u0aa1",              // Comma
    u8"\u0aa2",              // Period
    u8"\u0aa3",              // Slash
    u8"\u0020",              // Space
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
    u8"\u0aa4",              // BackQuote
    u8"\u0aa4\u0acd\u0ab0",  // Digit1
    u8"\u0aa5",              // Digit2
    u8"\u0aa6",              // Digit3
    u8"\u0aa7",              // Digit4
    u8"\u0aa8",              // Digit5
    u8"\u0aaa",              // Digit6
    u8"\u0aab",              // Digit7
    u8"\u0aac",              // Digit8
    u8"\u0aad",              // Digit9
    u8"\u0aae",              // Digit0
    u8"\u0aaf",              // Minus
    u8"\u0ab0",              // Equal
    u8"\u0ab2",              // KeyQ
    u8"\u0ab3",              // KeyW
    u8"\u0ab5",              // KeyE
    u8"\u0ab6",              // KeyR
    u8"\u0ab6\u0acd\u0ab0",  // KeyT
    u8"\u0ab7",              // KeyY
    u8"\u0ab8",              // KeyU
    u8"\u0ab9",              // KeyI
    u8"\u0abc",              // KeyO
    u8"\u0a81",              // KeyP
    u8"\u0a82",              // BracketLeft
    u8"\u0acd",              // BracketRight
    u8"\u0abe",              // Backslash
    u8"\u0abf",              // KeyA
    u8"\u0ac0",              // KeyS
    u8"\u0ac1",              // KeyD
    u8"\u0ac2",              // KeyF
    u8"\u0ac3",              // KeyG
    u8"\u0ac4",              // KeyH
    u8"\u0ae2",              // KeyJ
    u8"\u0ae3",              // KeyK
    u8"\u0ac5",              // KeyL
    u8"\u0ac7",              // Semicolon
    u8"\u0ac8",              // Quote
    u8"\u0ac9",              // KeyZ
    u8"\u0acb",              // KeyX
    u8"\u0acc",              // KeyC
    u8"\u0abd",              // KeyV
    u8"",                    // KeyB
    u8"",                    // KeyN
    u8"",                    // KeyM
    u8"",                    // Comma
    u8"",                    // Period
    u8"",                    // Slash
    u8"\u0020",              // Space
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
const char** kKeyMap[8] = {kNormal,     kShift,        kAltGr,
                           kShiftAltGr, kCapslock,     kShiftCapslock,
                           kCapslock,   kShiftCapslock};
const char* kTransforms[] = {
    u8"\u0acd\u001d?\\.c",
    u8"\u0ac5",
    u8"\u0a86\u0a8a\u001d?M",
    u8"\u0ad0",
    u8"\u0ab0\u0abc\u001d?\\^i",
    u8"\u0a8b",
    u8"\u0ab0\u0abc\u001d?\\^I",
    u8"\u0ae0",
    u8"\u0ab3\u001d?\\^i",
    u8"\u0a8c",
    u8"\u0ab3\u001d?\\^I",
    u8"\u0ae1",
    u8"\u0a9a\u001d?h",
    u8"\u0a9b",
    u8"\u0aa1\u0abc\u001d?h",
    u8"\u0aa2\u0abc",
    u8"\u0a95\u0acd\u0ab7\u001d?h",
    u8"\u0a95\u0acd\u0ab7",
    u8"\\.n",
    u8"\u0a82",
    u8"\\.m",
    u8"\u0a82",
    u8"\\.N",
    u8"\u0a81",
    u8"\\.h",
    u8"\u0acd\u200c",
    u8"\\.a",
    u8"\u0abd",
    u8"OM",
    u8"\u0ad0",
    u8"\u0a85\u001d?a",
    u8"\u0a86",
    u8"\u0a87\u001d?i",
    u8"\u0a88",
    u8"\u0a8f\u001d?e",
    u8"\u0a88",
    u8"\u0a89\u001d?u",
    u8"\u0a8a",
    u8"\u0a93\u001d?o",
    u8"\u0a8a",
    u8"\u0a85\u001d?i",
    u8"\u0a90",
    u8"\u0a85\u001d?u",
    u8"\u0a94",
    u8"\u0a95\u001d?h",
    u8"\u0a96",
    u8"\u0a97\u001d?h",
    u8"\u0a98",
    u8"~N",
    u8"\u0a99",
    u8"ch",
    u8"\u0a9a",
    u8"Ch",
    u8"\u0a9b",
    u8"\u0a9c\u001d?h",
    u8"\u0a9d",
    u8"~n",
    u8"\u0a9e",
    u8"\u0a9f\u001d?h",
    u8"\u0aa0",
    u8"\u0aa1\u001d?h",
    u8"\u0aa2",
    u8"\u0aa4\u001d?h",
    u8"\u0aa5",
    u8"\u0aa6\u001d?h",
    u8"\u0aa7",
    u8"\u0aaa\u001d?h",
    u8"\u0aab",
    u8"\u0aac\u001d?h",
    u8"\u0aad",
    u8"\u0ab8\u001d?h",
    u8"\u0ab6",
    u8"\u0ab6\u001d?h",
    u8"\u0ab7",
    u8"~h",
    u8"\u0acd\u0ab9",
    u8"Kh",
    u8"\u0a96\u0abc",
    u8"\\.D",
    u8"\u0aa1\u0abc",
    u8"\u0ab0\u0abc\u001d?h",
    u8"\u0aa2\u0abc",
    u8"\u0a95\u001d?S",
    u8"\u0a95\u0acd\u0ab7",
    u8"\u0a97\u0abc\u001d?Y",
    u8"\u0a9c\u0acd\u0a9e",
    u8"M",
    u8"\u0a82",
    u8"H",
    u8"\u0a83",
    u8"a",
    u8"\u0a85",
    u8"A",
    u8"\u0a86",
    u8"i",
    u8"\u0a87",
    u8"I",
    u8"\u0a88",
    u8"u",
    u8"\u0a89",
    u8"U",
    u8"\u0a8a",
    u8"e",
    u8"\u0a8f",
    u8"o",
    u8"\u0a93",
    u8"k",
    u8"\u0a95",
    u8"g",
    u8"\u0a97",
    u8"j",
    u8"\u0a9c",
    u8"T",
    u8"\u0a9f",
    u8"D",
    u8"\u0aa1",
    u8"N",
    u8"\u0aa3",
    u8"t",
    u8"\u0aa4",
    u8"d",
    u8"\u0aa6",
    u8"n",
    u8"\u0aa8",
    u8"p",
    u8"\u0aaa",
    u8"b",
    u8"\u0aac",
    u8"m",
    u8"\u0aae",
    u8"y",
    u8"\u0aaf",
    u8"r",
    u8"\u0ab0",
    u8"l",
    u8"\u0ab2",
    u8"L",
    u8"\u0ab3",
    u8"v",
    u8"\u0ab5",
    u8"w",
    u8"\u0ab5",
    u8"S",
    u8"\u0ab6",
    u8"s",
    u8"\u0ab8",
    u8"h",
    u8"\u0ab9",
    u8"R",
    u8"\u0ab0\u0abc",
    u8"q",
    u8"\u0a95\u0abc",
    u8"G",
    u8"\u0a97\u0abc",
    u8"z",
    u8"\u0a9c\u0abc",
    u8"J",
    u8"\u0a9c\u0abc",
    u8"f",
    u8"\u0aab\u0abc",
    u8"Y",
    u8"\u0aaf\u0abc",
    u8"x",
    u8"\u0a95\u0acd\u0ab7",
    u8"([\u0a95-\u0ab9])\u001da",
    u8"\\1",
    u8"([\u0a95-\u0ab9])\u001daa",
    u8"\\1\u0abe",
    u8"([\u0a95-\u0ab9])\u001dai",
    u8"\\1\u0ac8",
    u8"([\u0a95-\u0ab9])\u001dau",
    u8"\\1\u0acc",
    u8"([\u0a95-\u0ab9])\u001dA",
    u8"\\1\u0abe",
    u8"([\u0a95-\u0ab9])\u001di",
    u8"\\1\u0abf",
    u8"\u0abf\u001di",
    u8"\u0ac0",
    u8"\u0ac7\u001de",
    u8"\u0ac0",
    u8"([\u0a95-\u0ab9])\u001dI",
    u8"\\1\u0ac0",
    u8"([\u0a95-\u0ab9])\u001du",
    u8"\\1\u0ac1",
    u8"([\u0a95-\u0ab9])\u001dU",
    u8"\\1\u0ac2",
    u8"\u0ac1\u001du",
    u8"\u0ac2",
    u8"\u0acb\u001do",
    u8"\u0ac2",
    u8"([\u0a95-\u0ab9])\u0acd\u0ab0\u0abc\u0acd\u0ab0\u0abc\u001di",
    u8"\\1\u0ac3",
    u8"([\u0a95-\u0ab9])\u0acd\u0ab0\u0abc^i",
    u8"\\1\u0ac3",
    u8"([\u0a95-\u0ab9])\u0acd\u0ab0\u0abc\u0acd\u0ab0\u0abc\u001dI",
    u8"\\1\u0ac4",
    u8"([\u0a95-\u0ab9])\u0acd\u0ab0\u0abc^I",
    u8"\\1\u0ac4",
    u8"\u0ab0\u0abc\u0acd\u0ab0\u0abc\u001di",
    u8"\u0a8b",
    u8"\u0ab0\u0abc\u0acd\u0ab0\u0abc\u001dI",
    u8"\u0ae0",
    u8"\u0ab3\u0acd\u0ab3\u001di",
    u8"\u0a8c",
    u8"\u0ab3\u0acd\u0ab3\u001dI",
    u8"\u0ae1",
    u8"([\u0a95-\u0ab9])\u001de",
    u8"\\1\u0ac7",
    u8"([\u0a95-\u0ab9])\u001do",
    u8"\\1\u0acb",
    u8"([\u0a95-\u0ab9])\u001dk",
    u8"\\1\u0acd\u0a95",
    u8"([\u0a95-\u0ab9])\u001dg",
    u8"\\1\u0acd\u0a97",
    u8"([\u0a95-\u0ab9])\u001d~N",
    u8"\\1\u0acd\u0a99",
    u8"([\u0a95-\u0ab9])\u001dch",
    u8"\\1\u0acd\u0a9a",
    u8"([\u0a95-\u0ab9])\u001dCh",
    u8"\\1\u0acd\u0a9b",
    u8"([\u0a95-\u0ab9])\u001dj",
    u8"\\1\u0acd\u0a9c",
    u8"([\u0a95-\u0ab9])\u001d~n",
    u8"\\1\u0acd\u0a9e",
    u8"([\u0a95-\u0ab9])\u001dT",
    u8"\\1\u0acd\u0a9f",
    u8"([\u0a95-\u0ab9])\u001dD",
    u8"\\1\u0acd\u0aa1",
    u8"([\u0a95-\u0ab9])\u001dN",
    u8"\\1\u0acd\u0aa3",
    u8"([\u0a95-\u0ab9])\u001dt",
    u8"\\1\u0acd\u0aa4",
    u8"([\u0a95-\u0ab9])\u001dd",
    u8"\\1\u0acd\u0aa6",
    u8"([\u0a95-\u0ab9])\u001dn",
    u8"\\1\u0acd\u0aa8",
    u8"([\u0a95-\u0ab9])\u001dp",
    u8"\\1\u0acd\u0aaa",
    u8"([\u0a95-\u0ab9])\u001db",
    u8"\\1\u0acd\u0aac",
    u8"([\u0a95-\u0ab9])\u001dm",
    u8"\\1\u0acd\u0aae",
    u8"([\u0a95-\u0ab9])\u001dr",
    u8"\\1\u0acd\u0ab0",
    u8"([\u0a95-\u0ab9])\u001dl",
    u8"\\1\u0acd\u0ab2",
    u8"([\u0a95-\u0ab9])\u001dL",
    u8"\\1\u0acd\u0ab3",
    u8"([\u0a95-\u0ab9])\u001dv",
    u8"\\1\u0acd\u0ab5",
    u8"([\u0a95-\u0ab9])\u001dw",
    u8"\\1\u0acd\u0ab5",
    u8"([\u0a95-\u0ab9])\u001dS",
    u8"\\1\u0acd\u0ab6",
    u8"([\u0a95-\u0ab9])\u001ds",
    u8"\\1\u0acd\u0ab8",
    u8"([\u0a95-\u0ab9])\u001dh",
    u8"\\1\u0acd\u0ab9",
    u8"([\u0a95-\u0ab9])\u001dR",
    u8"\\1\u0acd\u0ab0\u0abc",
    u8"([\u0a95-\u0ab9])\u001dq",
    u8"\\1\u0acd\u0a95\u0abc",
    u8"([\u0a95-\u0ab9])\u001dKh",
    u8"\\1\u0acd\u0a96\u0abc",
    u8"([\u0a95-\u0ab9])\u001dG",
    u8"\\1\u0acd\u0a97\u0abc",
    u8"([\u0a95-\u0ab9])\u001dz",
    u8"\\1\u0acd\u0a9c\u0abc",
    u8"([\u0a95-\u0ab9])\u001dJ",
    u8"\\1\u0acd\u0a9c\u0abc",
    u8"([\u0a95-\u0ab9])\u001d.D",
    u8"\\1\u0acd\u0aa1\u0abc",
    u8"([\u0a95-\u0ab9])\u001df",
    u8"\\1\u0acd\u0aab\u0abc",
    u8"([\u0a95-\u0ab9])\u001dy",
    u8"\\1\u0acd\u0aaf\u0abc",
    u8"([\u0a95-\u0ab9])\u001dx",
    u8"\\1\u0acd\u0a95\u0acd\u0ab7",
    u8"([\u0a95-\u0ab9])\u001dak",
    u8"\\1\u0a95",
    u8"([\u0a95-\u0ab9])\u001dag",
    u8"\\1\u0a97",
    u8"([\u0a95-\u0ab9])\u001da~N",
    u8"\\1\u0a99",
    u8"([\u0a95-\u0ab9])\u001dach",
    u8"\\1\u0a9a",
    u8"([\u0a95-\u0ab9])\u001daCh",
    u8"\\1\u0a9b",
    u8"([\u0a95-\u0ab9])\u001daj",
    u8"\\1\u0a9c",
    u8"([\u0a95-\u0ab9])\u001da~n",
    u8"\\1\u0a9e",
    u8"([\u0a95-\u0ab9])\u001daT",
    u8"\\1\u0a9f",
    u8"([\u0a95-\u0ab9])\u001daD",
    u8"\\1\u0aa1",
    u8"([\u0a95-\u0ab9])\u001daN",
    u8"\\1\u0aa3",
    u8"([\u0a95-\u0ab9])\u001dat",
    u8"\\1\u0aa4",
    u8"([\u0a95-\u0ab9])\u001dad",
    u8"\\1\u0aa6",
    u8"([\u0a95-\u0ab9])\u001dan",
    u8"\\1\u0aa8",
    u8"([\u0a95-\u0ab9])\u001dap",
    u8"\\1\u0aaa",
    u8"([\u0a95-\u0ab9])\u001dab",
    u8"\\1\u0aac",
    u8"([\u0a95-\u0ab9])\u001dam",
    u8"\\1\u0aae",
    u8"([\u0a95-\u0ab9])\u001dar",
    u8"\\1\u0ab0",
    u8"([\u0a95-\u0ab9])\u001dal",
    u8"\\1\u0ab2",
    u8"([\u0a95-\u0ab9])\u001daL",
    u8"\\1\u0ab3",
    u8"([\u0a95-\u0ab9])\u001dav",
    u8"\\1\u0ab5",
    u8"([\u0a95-\u0ab9])\u001daw",
    u8"\\1\u0ab5",
    u8"([\u0a95-\u0ab9])\u001daS",
    u8"\\1\u0ab6",
    u8"([\u0a95-\u0ab9])\u001das",
    u8"\\1\u0ab8",
    u8"([\u0a95-\u0ab9])\u001dah",
    u8"\\1\u0ab9",
    u8"([\u0a95-\u0ab9])\u001daR",
    u8"\\1\u0ab0\u0abc",
    u8"([\u0a95-\u0ab9])\u001daq",
    u8"\\1\u0a95\u0abc",
    u8"([\u0a95-\u0ab9])\u001daKh",
    u8"\\1\u0a96\u0abc",
    u8"([\u0a95-\u0ab9])\u001daG",
    u8"\\1\u0a97\u0abc",
    u8"([\u0a95-\u0ab9])\u001daz",
    u8"\\1\u0a9c\u0abc",
    u8"([\u0a95-\u0ab9])\u001daJ",
    u8"\\1\u0a9c\u0abc",
    u8"([\u0a95-\u0ab9])\u001da.D",
    u8"\\1\u0aa1\u0abc",
    u8"([\u0a95-\u0ab9])\u001daf",
    u8"\\1\u0aab\u0abc",
    u8"([\u0a95-\u0ab9])\u001day",
    u8"\\1\u0aaf\u0abc",
    u8"([\u0a95-\u0ab9])\u001dax",
    u8"\\1\u0a95\u0acd\u0ab7",
    u8"([\u0a95-\u0ab9])\u001daak",
    u8"\\1\u0abe\u0a95",
    u8"([\u0a95-\u0ab9])\u001daag",
    u8"\\1\u0abe\u0a97",
    u8"([\u0a95-\u0ab9])\u001daa~N",
    u8"\\1\u0abe\u0a99",
    u8"([\u0a95-\u0ab9])\u001daach",
    u8"\\1\u0abe\u0a9a",
    u8"([\u0a95-\u0ab9])\u001daaCh",
    u8"\\1\u0abe\u0a9b",
    u8"([\u0a95-\u0ab9])\u001daaj",
    u8"\\1\u0abe\u0a9c",
    u8"([\u0a95-\u0ab9])\u001daa~n",
    u8"\\1\u0abe\u0a9e",
    u8"([\u0a95-\u0ab9])\u001daaT",
    u8"\\1\u0abe\u0a9f",
    u8"([\u0a95-\u0ab9])\u001daaD",
    u8"\\1\u0abe\u0aa1",
    u8"([\u0a95-\u0ab9])\u001daaN",
    u8"\\1\u0abe\u0aa3",
    u8"([\u0a95-\u0ab9])\u001daat",
    u8"\\1\u0abe\u0aa4",
    u8"([\u0a95-\u0ab9])\u001daad",
    u8"\\1\u0abe\u0aa6",
    u8"([\u0a95-\u0ab9])\u001daan",
    u8"\\1\u0abe\u0aa8",
    u8"([\u0a95-\u0ab9])\u001daap",
    u8"\\1\u0abe\u0aaa",
    u8"([\u0a95-\u0ab9])\u001daab",
    u8"\\1\u0abe\u0aac",
    u8"([\u0a95-\u0ab9])\u001daam",
    u8"\\1\u0abe\u0aae",
    u8"([\u0a95-\u0ab9])\u001daar",
    u8"\\1\u0abe\u0ab0",
    u8"([\u0a95-\u0ab9])\u001daal",
    u8"\\1\u0abe\u0ab2",
    u8"([\u0a95-\u0ab9])\u001daaL",
    u8"\\1\u0abe\u0ab3",
    u8"([\u0a95-\u0ab9])\u001daav",
    u8"\\1\u0abe\u0ab5",
    u8"([\u0a95-\u0ab9])\u001daaw",
    u8"\\1\u0abe\u0ab5",
    u8"([\u0a95-\u0ab9])\u001daaS",
    u8"\\1\u0abe\u0ab6",
    u8"([\u0a95-\u0ab9])\u001daas",
    u8"\\1\u0abe\u0ab8",
    u8"([\u0a95-\u0ab9])\u001daah",
    u8"\\1\u0abe\u0ab9",
    u8"([\u0a95-\u0ab9])\u001daaR",
    u8"\\1\u0abe\u0ab0\u0abc",
    u8"([\u0a95-\u0ab9])\u001daaq",
    u8"\\1\u0abe\u0a95\u0abc",
    u8"([\u0a95-\u0ab9])\u001daaKh",
    u8"\\1\u0abe\u0a96\u0abc",
    u8"([\u0a95-\u0ab9])\u001daaG",
    u8"\\1\u0abe\u0a97\u0abc",
    u8"([\u0a95-\u0ab9])\u001daaz",
    u8"\\1\u0abe\u0a9c\u0abc",
    u8"([\u0a95-\u0ab9])\u001daaJ",
    u8"\\1\u0abe\u0a9c\u0abc",
    u8"([\u0a95-\u0ab9])\u001daa.D",
    u8"\\1\u0abe\u0aa1\u0abc",
    u8"([\u0a95-\u0ab9])\u001daaf",
    u8"\\1\u0abe\u0aab\u0abc",
    u8"([\u0a95-\u0ab9])\u001daay",
    u8"\\1\u0abe\u0aaf\u0abc",
    u8"([\u0a95-\u0ab9])\u001daax",
    u8"\\1\u0abe\u0a95\u0acd\u0ab7"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune =
    "a|aa|ac|aaC|aac|a\\.|aK|aC|aaK|aS|aaS|aa~|aa\\.|a~";

}  // namespace gu_phone
