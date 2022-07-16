// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/ta_typewriter.h"

namespace ta_typewriter {

const char* kId = "ta_typewriter";
bool kIs102 = false;
const char* kNormal[] = {
    u8"\u0b83",              // BackQuote
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
    u8"/",                   // Minus
    u8"=",                   // Equal
    u8"\u0ba3\u0bc1",        // KeyQ
    u8"\u0bb1",              // KeyW
    u8"\u0ba8",              // KeyE
    u8"\u0b9a",              // KeyR
    u8"\u0bb5",              // KeyT
    u8"\u0bb2",              // KeyY
    u8"\u0bb0",              // KeyU
    u8"\u0bc8",              // KeyI
    u8"\u0b9f\u0bbf",        // KeyO
    u8"\u0bbf",              // KeyP
    u8"\u0bc1",              // BracketLeft
    u8"\u0bb9",              // BracketRight
    u8"\u0b95\u0bcd\u0bb7",  // Backslash
    u8"\u0baf",              // KeyA
    u8"\u0bb3",              // KeyS
    u8"\u0ba9",              // KeyD
    u8"\u0b95",              // KeyF
    u8"\u0baa",              // KeyG
    u8"\u0bbe",              // KeyH
    u8"\u0ba4",              // KeyJ
    u8"\u0bae",              // KeyK
    u8"\u0b9f",              // KeyL
    u8"\u0bcd",              // Semicolon
    u8"\u0b99",              // Quote
    u8"\u0ba3",              // KeyZ
    u8"\u0b92",              // KeyX
    u8"\u0b89",              // KeyC
    u8"\u0b8e",              // KeyV
    u8"\u0bc6",              // KeyB
    u8"\u0bc7",              // KeyN
    u8"\u0b85",              // KeyM
    u8"\u0b87",              // Comma
    u8",",                   // Period
    u8".",                   // Slash
    u8"\u0020",              // Space
};
const char* kShift[] = {
    u8"'",                         // BackQuote
    u8"\u0bb8",                    // Digit1
    u8"\"",                        // Digit2
    u8"%",                         // Digit3
    u8"\u0b9c",                    // Digit4
    u8"\u0bb6",                    // Digit5
    u8"\u0bb7",                    // Digit6
    u8"",                          // Digit7
    u8"",                          // Digit8
    u8"(",                         // Digit9
    u8")",                         // Digit0
    u8"\u0bb8\u0bcd\u0bb0\u0bc0",  // Minus
    u8"+",                         // Equal
    u8"",                          // KeyQ
    u8"\u0bb1\u0bc1",              // KeyW
    u8"\u0ba8\u0bc1",              // KeyE
    u8"\u0b9a\u0bc1",              // KeyR
    u8"\u0b95\u0bc2",              // KeyT
    u8"\u0bb2\u0bc1",              // KeyY
    u8"\u0bb0\u0bc1",              // KeyU
    u8"\u0b90",                    // KeyI
    u8"\u0b9f\u0bc0",              // KeyO
    u8"\u0bc0",                    // KeyP
    u8"\u0bc2",                    // BracketLeft
    u8"\u0bcc",                    // BracketRight
    u8"\u0bf8",                    // Backslash
    u8"",                          // KeyA
    u8"\u0bb3\u0bc1",              // KeyS
    u8"\u0ba9\u0bc1",              // KeyD
    u8"\u0b95\u0bc1",              // KeyF
    u8"\u0bb4\u0bc1",              // KeyG
    u8"\u0bb4",                    // KeyH
    u8"\u0ba4\u0bc1",              // KeyJ
    u8"\u0bae\u0bc1",              // KeyK
    u8"\u0b9f\u0bc1",              // KeyL
    u8"\\",                        // Semicolon
    u8"\u0b9e",                    // Quote
    u8"\u0bb7",                    // KeyZ
    u8"\u0b93",                    // KeyX
    u8"\u0b8a",                    // KeyC
    u8"\u0b8f",                    // KeyV
    u8"\u0b95\u0bcd\u0bb7",        // KeyB
    u8"\u0b9a\u0bc2",              // KeyN
    u8"\u0b86",                    // KeyM
    u8"\u0b88",                    // Comma
    u8"?",                         // Period
    u8"-",                         // Slash
    u8"\u0020",                    // Space
};
const char* kCapslock[] = {
    u8"'",                         // BackQuote
    u8"\u0bb8",                    // Digit1
    u8"\"",                        // Digit2
    u8"%",                         // Digit3
    u8"\u0b9c",                    // Digit4
    u8"\u0bb6",                    // Digit5
    u8"\u0bb7",                    // Digit6
    u8"",                          // Digit7
    u8"",                          // Digit8
    u8"(",                         // Digit9
    u8")",                         // Digit0
    u8"\u0bb8\u0bcd\u0bb0\u0bc0",  // Minus
    u8"+",                         // Equal
    u8"",                          // KeyQ
    u8"\u0bb1\u0bc1",              // KeyW
    u8"\u0ba8\u0bc1",              // KeyE
    u8"\u0b9a\u0bc1",              // KeyR
    u8"\u0b95\u0bc2",              // KeyT
    u8"\u0bb2\u0bc1",              // KeyY
    u8"\u0bb0\u0bc1",              // KeyU
    u8"\u0b90",                    // KeyI
    u8"\u0b9f\u0bc0",              // KeyO
    u8"\u0bc0",                    // KeyP
    u8"\u0bc2",                    // BracketLeft
    u8"\u0bcc",                    // BracketRight
    u8"\u0bf8",                    // Backslash
    u8"",                          // KeyA
    u8"\u0bb3\u0bc1",              // KeyS
    u8"\u0ba9\u0bc1",              // KeyD
    u8"\u0b95\u0bc1",              // KeyF
    u8"\u0bb4\u0bc1",              // KeyG
    u8"\u0bb4",                    // KeyH
    u8"\u0ba4\u0bc1",              // KeyJ
    u8"\u0bae\u0bc1",              // KeyK
    u8"\u0b9f\u0bc1",              // KeyL
    u8"\\",                        // Semicolon
    u8"\u0b9e",                    // Quote
    u8"\u0bb7",                    // KeyZ
    u8"\u0b93",                    // KeyX
    u8"\u0b8a",                    // KeyC
    u8"\u0b8f",                    // KeyV
    u8"\u0b95\u0bcd\u0bb7",        // KeyB
    u8"\u0b9a\u0bc2",              // KeyN
    u8"\u0b86",                    // KeyM
    u8"\u0b88",                    // Comma
    u8"?",                         // Period
    u8"-",                         // Slash
    u8"\u0020",                    // Space
};
const char* kShiftCapslock[] = {
    u8"\u0b83",              // BackQuote
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
    u8"/",                   // Minus
    u8"=",                   // Equal
    u8"\u0ba3\u0bc1",        // KeyQ
    u8"\u0bb1",              // KeyW
    u8"\u0ba8",              // KeyE
    u8"\u0b9a",              // KeyR
    u8"\u0bb5",              // KeyT
    u8"\u0bb2",              // KeyY
    u8"\u0bb0",              // KeyU
    u8"\u0bc8",              // KeyI
    u8"\u0b9f\u0bbf",        // KeyO
    u8"\u0bbf",              // KeyP
    u8"\u0bc1",              // BracketLeft
    u8"\u0bb9",              // BracketRight
    u8"\u0b95\u0bcd\u0bb7",  // Backslash
    u8"\u0baf",              // KeyA
    u8"\u0bb3",              // KeyS
    u8"\u0ba9",              // KeyD
    u8"\u0b95",              // KeyF
    u8"\u0baa",              // KeyG
    u8"\u0bbe",              // KeyH
    u8"\u0ba4",              // KeyJ
    u8"\u0bae",              // KeyK
    u8"\u0b9f",              // KeyL
    u8"\u0bcd",              // Semicolon
    u8"\u0b99",              // Quote
    u8"\u0ba3",              // KeyZ
    u8"\u0b92",              // KeyX
    u8"\u0b89",              // KeyC
    u8"\u0b8e",              // KeyV
    u8"\u0bc6",              // KeyB
    u8"\u0bc7",              // KeyN
    u8"\u0b85",              // KeyM
    u8"\u0b87",              // Comma
    u8",",                   // Period
    u8".",                   // Slash
    u8"\u0020",              // Space
};
const char** kKeyMap[8] = {kNormal,   kShift,        kNormal,
                           kShift,    kCapslock,     kShiftCapslock,
                           kCapslock, kShiftCapslock};

}  // namespace ta_typewriter
