// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/bn_phone.h"

#include <iterator>

namespace bn_phone {

const char* kId = "bn_phone";
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
    u8"\u0982",  // BackQuote
    u8"\u0981",  // Digit1
    u8"\u09bc",  // Digit2
    u8"\u0983",  // Digit3
    u8"\u09fa",  // Digit4
    u8"\u09f8",  // Digit5
    u8"\u09f9",  // Digit6
    u8"\u09f2",  // Digit7
    u8"\u09f3",  // Digit8
    u8"\u09e6",  // Digit9
    u8"\u09f4",  // Digit0
    u8"\u09e7",  // Minus
    u8"\u09f5",  // Equal
    u8"\u09e8",  // KeyQ
    u8"\u09f6",  // KeyW
    u8"\u09e9",  // KeyE
    u8"\u09f7",  // KeyR
    u8"\u09ea",  // KeyT
    u8"\u09eb",  // KeyY
    u8"\u09ec",  // KeyU
    u8"\u09ed",  // KeyI
    u8"\u09ee",  // KeyO
    u8"\u09ef",  // KeyP
    u8"\u0985",  // BracketLeft
    u8"\u0986",  // BracketRight
    u8"\u0987",  // Backslash
    u8"\u0988",  // KeyA
    u8"\u0989",  // KeyS
    u8"\u098a",  // KeyD
    u8"\u098b",  // KeyF
    u8"\u09e0",  // KeyG
    u8"\u098c",  // KeyH
    u8"\u09e1",  // KeyJ
    u8"\u098f",  // KeyK
    u8"\u0990",  // KeyL
    u8"\u0993",  // Semicolon
    u8"\u0994",  // Quote
    u8"\u0995",  // KeyZ
    u8"\u0996",  // KeyX
    u8"\u0997",  // KeyC
    u8"\u0998",  // KeyV
    u8"\u0999",  // KeyB
    u8"\u099a",  // KeyN
    u8"\u099b",  // KeyM
    u8"\u099c",  // Comma
    u8"\u099d",  // Period
    u8"\u099e",  // Slash
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
    u8"\u099f",  // BackQuote
    u8"\u09a0",  // Digit1
    u8"\u09a1",  // Digit2
    u8"\u09dc",  // Digit3
    u8"\u09a2",  // Digit4
    u8"\u09dd",  // Digit5
    u8"\u09a3",  // Digit6
    u8"\u09a4",  // Digit7
    u8"\u09ce",  // Digit8
    u8"\u09a5",  // Digit9
    u8"\u09a6",  // Digit0
    u8"\u09a7",  // Minus
    u8"\u09a8",  // Equal
    u8"\u09aa",  // KeyQ
    u8"\u09ab",  // KeyW
    u8"\u09ac",  // KeyE
    u8"\u09ad",  // KeyR
    u8"\u09ae",  // KeyT
    u8"\u09af",  // KeyY
    u8"\u09df",  // KeyU
    u8"\u09b0",  // KeyI
    u8"\u09f0",  // KeyO
    u8"\u09b2",  // KeyP
    u8"\u09f1",  // BracketLeft
    u8"\u09b6",  // BracketRight
    u8"\u09b7",  // Backslash
    u8"\u09b8",  // KeyA
    u8"\u09b9",  // KeyS
    u8"\u09bd",  // KeyD
    u8"\u09be",  // KeyF
    u8"\u09bf",  // KeyG
    u8"\u09c0",  // KeyH
    u8"\u09c1",  // KeyJ
    u8"\u09c2",  // KeyK
    u8"\u09c3",  // KeyL
    u8"\u09c4",  // Semicolon
    u8"\u09e2",  // Quote
    u8"\u09e3",  // KeyZ
    u8"\u09c7",  // KeyX
    u8"\u09c8",  // KeyC
    u8"\u09cb",  // KeyV
    u8"\u09cc",  // KeyB
    u8"\u09cd",  // KeyN
    u8"\u09d7",  // KeyM
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
const char** kKeyMap[8] = {kNormal,     kShift,        kAltGr,
                           kShiftAltGr, kCapslock,     kShiftCapslock,
                           kCapslock,   kShiftCapslock};
const char* kTransforms[] = {
    u8"\u09cd\u001d?\\.c",
    u8"\u09c7",
    u8"\u0986\u098a\u001d?M",
    u8"\u0993\u0982",
    u8"\u09dc\u001d?\\^i",
    u8"\u098b",
    u8"\u09dc\u001d?\\^I",
    u8"\u09e0",
    u8"\u09b2\u001d?\\^i",
    u8"\u098c",
    u8"\u09b2\u001d?\\^I",
    u8"\u09e1",
    u8"\u099a\u001d?h",
    u8"\u099b",
    u8"\u0995\u09cd\u09b7\u001d?h",
    u8"\u0995\u09cd\u09b7",
    u8"\\.n",
    u8"\u0982",
    u8"\\.m",
    u8"\u0982",
    u8"\\.N",
    u8"\u0981",
    u8"\\.h",
    u8"\u09cd\u200c",
    u8"\\.a",
    u8"\u09bd",
    u8"OM",
    u8"\u0993\u0982",
    u8"\u0985\u001d?a",
    u8"\u0986",
    u8"\u0987\u001d?i",
    u8"\u0988",
    u8"\u098f\u001d?e",
    u8"\u0988",
    u8"\u0989\u001d?u",
    u8"\u098a",
    u8"\u0993\u001d?o",
    u8"\u098a",
    u8"\u0985\u001d?i",
    u8"\u0990",
    u8"\u0985\u001d?u",
    u8"\u0994",
    u8"\u0995\u001d?h",
    u8"\u0996",
    u8"\u0997\u001d?h",
    u8"\u0998",
    u8"~N",
    u8"\u0999",
    u8"ch",
    u8"\u099a",
    u8"Ch",
    u8"\u099b",
    u8"\u099c\u001d?h",
    u8"\u099d",
    u8"~n",
    u8"\u099e",
    u8"\u099f\u001d?h",
    u8"\u09a0",
    u8"\u09a1\u001d?h",
    u8"\u09a2",
    u8"\u09a4\u001d?h",
    u8"\u09a5",
    u8"\u09a6\u001d?h",
    u8"\u09a7",
    u8"\u09aa\u001d?h",
    u8"\u09ab",
    u8"\u09ac\u001d?h",
    u8"\u09ad",
    u8"\u09b8\u001d?h",
    u8"\u09b6",
    u8"\u09b6\u001d?h",
    u8"\u09b7",
    u8"~h",
    u8"\u09cd\u09b9",
    u8"\u09dc\u001d?h",
    u8"\u09dd",
    u8"\u0995\u001d?S",
    u8"\u0995\u09cd\u09b7",
    u8"GY",
    u8"\u099c\u09cd\u099e",
    u8"M",
    u8"\u0982",
    u8"H",
    u8"\u0983",
    u8"a",
    u8"\u0985",
    u8"A",
    u8"\u0986",
    u8"i",
    u8"\u0987",
    u8"I",
    u8"\u0988",
    u8"u",
    u8"\u0989",
    u8"U",
    u8"\u098a",
    u8"e",
    u8"\u098f",
    u8"o",
    u8"\u0993",
    u8"k",
    u8"\u0995",
    u8"g",
    u8"\u0997",
    u8"j",
    u8"\u099c",
    u8"T",
    u8"\u099f",
    u8"D",
    u8"\u09a1",
    u8"N",
    u8"\u09a3",
    u8"t",
    u8"\u09a4",
    u8"d",
    u8"\u09a6",
    u8"n",
    u8"\u09a8",
    u8"p",
    u8"\u09aa",
    u8"b",
    u8"\u09ac",
    u8"m",
    u8"\u09ae",
    u8"y",
    u8"\u09af",
    u8"r",
    u8"\u09b0",
    u8"l",
    u8"\u09b2",
    u8"L",
    u8"\u09b2",
    u8"v",
    u8"\u09ac",
    u8"w",
    u8"\u09ac",
    u8"S",
    u8"\u09b6",
    u8"s",
    u8"\u09b8",
    u8"h",
    u8"\u09b9",
    u8"R",
    u8"\u09dc",
    u8"Y",
    u8"\u09df",
    u8"x",
    u8"\u0995\u09cd\u09b7",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001da",
    u8"\\1",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daa",
    u8"\\1\u09be",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dai",
    u8"\\1\u09c8",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dau",
    u8"\\1\u09cc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dA",
    u8"\\1\u09be",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001di",
    u8"\\1\u09bf",
    u8"\u09bf\u001di",
    u8"\u09c0",
    u8"\u09c7\u001de",
    u8"\u09c0",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dI",
    u8"\\1\u09c0",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001du",
    u8"\\1\u09c1",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dU",
    u8"\\1\u09c2",
    u8"\u09c1\u001du",
    u8"\u09c2",
    u8"\u09cb\u001do",
    u8"\u09c2",
    u8"([\u0995-\u09b9\u09dc-\u09df])"
    u8"\u09cd\u09b0\u09bc\u09cd\u09b0\u09bc\u001di",
    u8"\\1\u09c3",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u09cd\u09b0\u09bc^i",
    u8"\\1\u09c3",
    u8"([\u0995-\u09b9\u09dc-\u09df])"
    u8"\u09cd\u09b0\u09bc\u09cd\u09b0\u09bc\u001dI",
    u8"\\1\u09c4",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u09cd\u09b0\u09bc^I",
    u8"\\1\u09c4",
    u8"\u09b0\u09bc\u09cd\u09b0\u09bc\u001di",
    u8"\u098b",
    u8"\u09b0\u09bc\u09cd\u09b0\u09bc\u001dI",
    u8"\u09e0",
    u8"\u09b2\u09cd\u09b2\u001di",
    u8"\u098c",
    u8"\u09b2\u09cd\u09b2\u001dI",
    u8"\u09e1",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001de",
    u8"\\1\u09c7",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001do",
    u8"\\1\u09cb",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dk",
    u8"\\1\u09cd\u0995",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dg",
    u8"\\1\u09cd\u0997",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001d~N",
    u8"\\1\u09cd\u0999",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dch",
    u8"\\1\u09cd\u099a",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dCh",
    u8"\\1\u09cd\u099b",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dj",
    u8"\\1\u09cd\u099c",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001d~n",
    u8"\\1\u09cd\u099e",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dT",
    u8"\\1\u09cd\u099f",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dD",
    u8"\\1\u09cd\u09a1",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dN",
    u8"\\1\u09cd\u09a3",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dt",
    u8"\\1\u09cd\u09a4",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dd",
    u8"\\1\u09cd\u09a6",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dn",
    u8"\\1\u09cd\u09a8",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dp",
    u8"\\1\u09cd\u09aa",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001db",
    u8"\\1\u09cd\u09ac",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dm",
    u8"\\1\u09cd\u09ae",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dr",
    u8"\\1\u09cd\u09b0",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dl",
    u8"\\1\u09cd\u09b2",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dL",
    u8"\\1\u09cd\u09b2",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dv",
    u8"\\1\u09cd\u09ac",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dw",
    u8"\\1\u09cd\u09ac",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dS",
    u8"\\1\u09cd\u09b6",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001ds",
    u8"\\1\u09cd\u09b8",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dh",
    u8"\\1\u09cd\u09b9",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dR",
    u8"\\1\u09cd\u09b0\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dq",
    u8"\\1\u09cd\u0995\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dKh",
    u8"\\1\u09cd\u0996\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dG",
    u8"\\1\u09cd\u0997\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dz",
    u8"\\1\u09cd\u099c\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dJ",
    u8"\\1\u09cd\u099c\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001d.D",
    u8"\\1\u09cd\u09a1\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001df",
    u8"\\1\u09cd\u09ab\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dy",
    u8"\\1\u09cd\u09af\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dx",
    u8"\\1\u09cd\u0995\u09cd\u09b7",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dak",
    u8"\\1\u0995",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dag",
    u8"\\1\u0997",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001da~N",
    u8"\\1\u0999",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dach",
    u8"\\1\u099a",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daCh",
    u8"\\1\u099b",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daj",
    u8"\\1\u099c",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001da~n",
    u8"\\1\u099e",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daT",
    u8"\\1\u099f",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daD",
    u8"\\1\u09a1",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daN",
    u8"\\1\u09a3",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dat",
    u8"\\1\u09a4",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dad",
    u8"\\1\u09a6",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dan",
    u8"\\1\u09a8",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dap",
    u8"\\1\u09aa",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dab",
    u8"\\1\u09ac",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dam",
    u8"\\1\u09ae",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dar",
    u8"\\1\u09b0",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dal",
    u8"\\1\u09b2",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daL",
    u8"\\1\u09b2",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dav",
    u8"\\1\u09ac",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daw",
    u8"\\1\u09ac",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daS",
    u8"\\1\u09b6",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001das",
    u8"\\1\u09b8",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dah",
    u8"\\1\u09b9",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daR",
    u8"\\1\u09b0\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daq",
    u8"\\1\u0995\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daKh",
    u8"\\1\u0996\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daG",
    u8"\\1\u0997\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daz",
    u8"\\1\u099c\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daJ",
    u8"\\1\u099c\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001da.D",
    u8"\\1\u09a1\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daf",
    u8"\\1\u09ab\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001day",
    u8"\\1\u09af\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001dax",
    u8"\\1\u0995\u09cd\u09b7",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daak",
    u8"\\1\u09be\u0995",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daag",
    u8"\\1\u09be\u0997",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daa~N",
    u8"\\1\u09be\u0999",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daach",
    u8"\\1\u09be\u099a",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaCh",
    u8"\\1\u09be\u099b",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaj",
    u8"\\1\u09be\u099c",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daa~n",
    u8"\\1\u09be\u099e",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaT",
    u8"\\1\u09be\u099f",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaD",
    u8"\\1\u09be\u09a1",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaN",
    u8"\\1\u09be\u09a3",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daat",
    u8"\\1\u09be\u09a4",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daad",
    u8"\\1\u09be\u09a6",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daan",
    u8"\\1\u09be\u09a8",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daap",
    u8"\\1\u09be\u09aa",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daab",
    u8"\\1\u09be\u09ac",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daam",
    u8"\\1\u09be\u09ae",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daar",
    u8"\\1\u09be\u09b0",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daal",
    u8"\\1\u09be\u09b2",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaL",
    u8"\\1\u09be\u09b2",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daav",
    u8"\\1\u09be\u09ac",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaw",
    u8"\\1\u09be\u09ac",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaS",
    u8"\\1\u09be\u09b6",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daas",
    u8"\\1\u09be\u09b8",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daah",
    u8"\\1\u09be\u09b9",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaR",
    u8"\\1\u09be\u09b0\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaq",
    u8"\\1\u09be\u0995\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaKh",
    u8"\\1\u09be\u0996\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaG",
    u8"\\1\u09be\u0997\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaz",
    u8"\\1\u09be\u099c\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaJ",
    u8"\\1\u09be\u099c\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daa.D",
    u8"\\1\u09be\u09a1\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daaf",
    u8"\\1\u09be\u09ab\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daay",
    u8"\\1\u09be\u09af\u09bc",
    u8"([\u0995-\u09b9\u09dc-\u09df])\u001daax",
    u8"\\1\u09be\u0995\u09cd\u09b7"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune =
    "a|aa|ac|aaC|aac|a\\.|aK|aC|aaK|aS|aaS|aa~|aa\\.|a~";

}  // namespace bn_phone
