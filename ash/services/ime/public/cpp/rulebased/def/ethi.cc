// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/cpp/rulebased/def/ethi.h"

#include <iterator>

namespace ethi {

const char* kId = "ethi";
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
    u8"\u1245",  // KeyQ
    u8"\u12cd",  // KeyW
    u8"\u12a5",  // KeyE
    u8"\u122d",  // KeyR
    u8"\u1275",  // KeyT
    u8"\u12ed",  // KeyY
    u8"\u12a1",  // KeyU
    u8"\u12a2",  // KeyI
    u8"\u12a6",  // KeyO
    u8"\u1355",  // KeyP
    u8"[",       // BracketLeft
    u8"]",       // BracketRight
    u8"\\",      // Backslash
    u8"\u12a0",  // KeyA
    u8"\u1235",  // KeyS
    u8"\u12f5",  // KeyD
    u8"\u134d",  // KeyF
    u8"\u130d",  // KeyG
    u8"\u1205",  // KeyH
    u8"\u1305",  // KeyJ
    u8"\u12ad",  // KeyK
    u8"\u120d",  // KeyL
    u8"\u1364",  // Semicolon
    u8"'",       // Quote
    u8"\u12dd",  // KeyZ
    u8"\u123d",  // KeyX
    u8"\u127d",  // KeyC
    u8"\u126d",  // KeyV
    u8"\u1265",  // KeyB
    u8"\u1295",  // KeyN
    u8"\u121d",  // KeyM
    u8"\u1363",  // Comma
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
    u8"\u1245",  // KeyQ
    u8"\u12cd",  // KeyW
    u8"\u12a4",  // KeyE
    u8"\u122d",  // KeyR
    u8"\u1325",  // KeyT
    u8"\u12ed",  // KeyY
    u8"\u12a1",  // KeyU
    u8"\u12a5",  // KeyI
    u8"\u12a6",  // KeyO
    u8"\u1335",  // KeyP
    u8"{",       // BracketLeft
    u8"}",       // BracketRight
    u8"|",       // Backslash
    u8"\u12d0",  // KeyA
    u8"\u133d",  // KeyS
    u8"\u12f5",  // KeyD
    u8"\u134d",  // KeyF
    u8"\u130d",  // KeyG
    u8"\u1215",  // KeyH
    u8"\u1305",  // KeyJ
    u8"\u12bd",  // KeyK
    u8"\u120d",  // KeyL
    u8"\u1361",  // Semicolon
    u8"\"",      // Quote
    u8"\u12e5",  // KeyZ
    u8"\u123d",  // KeyX
    u8"\u132d",  // KeyC
    u8"\u126d",  // KeyV
    u8"\u1265",  // KeyB
    u8"\u129d",  // KeyN
    u8"\u121d",  // KeyM
    u8"\u2039",  // Comma
    u8"\u203a",  // Period
    u8"?",       // Slash
    u8"\u0020",  // Space
};
const char* kAltGr[] = {
    u8"`",       // BackQuote
    u8"\u1369",  // Digit1
    u8"\u136a",  // Digit2
    u8"\u136b",  // Digit3
    u8"\u136c",  // Digit4
    u8"\u136d",  // Digit5
    u8"\u136e",  // Digit6
    u8"\u136f",  // Digit7
    u8"\u1370",  // Digit8
    u8"\u1371",  // Digit9
    u8"0",       // Digit0
    u8"-",       // Minus
    u8"=",       // Equal
    u8"\u1255",  // KeyQ
    u8"\u12cd",  // KeyW
    u8"\u12a5",  // KeyE
    u8"\u122d",  // KeyR
    u8"\u1275",  // KeyT
    u8"\u12ed",  // KeyY
    u8"\u12a1",  // KeyU
    u8"\u12a2",  // KeyI
    u8"\u12a6",  // KeyO
    u8"\u1355",  // KeyP
    u8"[",       // BracketLeft
    u8"]",       // BracketRight
    u8"\\",      // Backslash
    u8"\u12a0",  // KeyA
    u8"\u1225",  // KeyS
    u8"\u12fd",  // KeyD
    u8"\u134d",  // KeyF
    u8"\u131d",  // KeyG
    u8"\u1285",  // KeyH
    u8"\u1305",  // KeyJ
    u8"\u2dcd",  // KeyK
    u8"\u120d",  // KeyL
    u8"\u1364",  // Semicolon
    u8"'",       // Quote
    u8"\u2db5",  // KeyZ
    u8"\u2da5",  // KeyX
    u8"\u2dad",  // KeyC
    u8"\u126d",  // KeyV
    u8"\u1265",  // KeyB
    u8"\u1295",  // KeyN
    u8"\u121d",  // KeyM
    u8"\u1363",  // Comma
    u8".",       // Period
    u8"/",       // Slash
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
    u8"\u1245",  // KeyQ
    u8"\u12cd",  // KeyW
    u8"\u12a4",  // KeyE
    u8"\u122d",  // KeyR
    u8"\u1325",  // KeyT
    u8"\u12ed",  // KeyY
    u8"\u12a1",  // KeyU
    u8"\u12a5",  // KeyI
    u8"\u12a6",  // KeyO
    u8"\u1335",  // KeyP
    u8"[",       // BracketLeft
    u8"]",       // BracketRight
    u8"\\",      // Backslash
    u8"\u12d0",  // KeyA
    u8"\u133d",  // KeyS
    u8"\u12f5",  // KeyD
    u8"\u134d",  // KeyF
    u8"\u130d",  // KeyG
    u8"\u1215",  // KeyH
    u8"\u1305",  // KeyJ
    u8"\u12bd",  // KeyK
    u8"\u120d",  // KeyL
    u8"\u1361",  // Semicolon
    u8"'",       // Quote
    u8"\u12e5",  // KeyZ
    u8"\u123d",  // KeyX
    u8"\u132d",  // KeyC
    u8"\u126d",  // KeyV
    u8"\u1265",  // KeyB
    u8"\u129d",  // KeyN
    u8"\u121d",  // KeyM
    u8",",       // Comma
    u8".",       // Period
    u8"/",       // Slash
    u8"\u0020",  // Space
};
const char* kShiftAltGr[] = {
    u8"~",       // BackQuote
    u8"\u1369",  // Digit1
    u8"\u136a",  // Digit2
    u8"\u136b",  // Digit3
    u8"\u136c",  // Digit4
    u8"\u136d",  // Digit5
    u8"\u136e",  // Digit6
    u8"\u136f",  // Digit7
    u8"\u1370",  // Digit8
    u8"\u1371",  // Digit9
    u8"0",       // Digit0
    u8"_",       // Minus
    u8"+",       // Equal
    u8"\u2dc5",  // KeyQ
    u8"\u12cd",  // KeyW
    u8"\u12a4",  // KeyE
    u8"\u122d",  // KeyR
    u8"\u1325",  // KeyT
    u8"\u12ed",  // KeyY
    u8"\u12a1",  // KeyU
    u8"\u12a5",  // KeyI
    u8"\u12a6",  // KeyO
    u8"\u1335",  // KeyP
    u8"{",       // BracketLeft
    u8"}",       // BracketRight
    u8"|",       // Backslash
    u8"\u12d0",  // KeyA
    u8"\u1345",  // KeyS
    u8"\u12fd",  // KeyD
    u8"\u134d",  // KeyF
    u8"\u2ddd",  // KeyG
    u8"\u1285",  // KeyH
    u8"\u1305",  // KeyJ
    u8"\u2dd5",  // KeyK
    u8"\u120d",  // KeyL
    u8"\u1361",  // Semicolon
    u8"\"",      // Quote
    u8"\u2db5",  // KeyZ
    u8"\u2da5",  // KeyX
    u8"\u2dbd",  // KeyC
    u8"\u126d",  // KeyV
    u8"\u1265",  // KeyB
    u8"\u129d",  // KeyN
    u8"\u121d",  // KeyM
    u8"\u2039",  // Comma
    u8"\u203a",  // Period
    u8"?",       // Slash
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
    u8"\u1245",  // KeyQ
    u8"\u12cd",  // KeyW
    u8"\u12a5",  // KeyE
    u8"\u122d",  // KeyR
    u8"\u1275",  // KeyT
    u8"\u12ed",  // KeyY
    u8"\u12a1",  // KeyU
    u8"\u12a2",  // KeyI
    u8"\u12a6",  // KeyO
    u8"\u1355",  // KeyP
    u8"{",       // BracketLeft
    u8"}",       // BracketRight
    u8"|",       // Backslash
    u8"\u12a0",  // KeyA
    u8"\u1235",  // KeyS
    u8"\u12f5",  // KeyD
    u8"\u134d",  // KeyF
    u8"\u130d",  // KeyG
    u8"\u1205",  // KeyH
    u8"\u1305",  // KeyJ
    u8"\u12ad",  // KeyK
    u8"\u120d",  // KeyL
    u8"\u1364",  // Semicolon
    u8"\"",      // Quote
    u8"\u12dd",  // KeyZ
    u8"\u123d",  // KeyX
    u8"\u127d",  // KeyC
    u8"\u126d",  // KeyV
    u8"\u1265",  // KeyB
    u8"\u1295",  // KeyN
    u8"\u121d",  // KeyM
    u8"<",       // Comma
    u8">",       // Period
    u8"?",       // Slash
    u8"\u0020",  // Space
};
const char** kKeyMap[8] = {kNormal,     kShift,        kAltGr,
                           kShiftAltGr, kCapslock,     kShiftCapslock,
                           kCapslock,   kShiftCapslock};
const char* kTransforms[] = {u8"\u128a\u001d?\u12a5",
                             u8"\u128c",
                             u8"\u1221\u001d?\u12a0",
                             u8"\u1227",
                             u8"\u1222\u001d?\u12a5",
                             u8"\u1224",
                             u8"\u124a\u001d?\u12a5",
                             u8"\u124c",
                             u8"\u125a\u001d?\u12a5",
                             u8"\u125c",
                             u8"\u1281\u001d?\u12a0",
                             u8"\u128b",
                             u8"\u1281\u001d?\u12a1\u001d?",
                             u8"\u128d",
                             u8"\u1281\u001d?\u12a2\u001d?",
                             u8"\u128a",
                             u8"\u1281\u001d?\u12a5",
                             u8"\u1288",
                             u8"\u1281\u001d?\u12a4\u001d?",
                             u8"\u128c",
                             u8"\u1282\u001d?\u12a5",
                             u8"\u1284",
                             u8"\u1284\u001d?\u12a4\u001d?",
                             u8"\u1288",
                             u8"\u1280\u001d?\u12a6\u001d?",
                             u8"\u1287",
                             u8"\u1286\u001d?\u12a0",
                             u8"\u1287",
                             u8"\u12d2\u001d?\u12a5",
                             u8"\u12d4",
                             u8"\u12b2\u001d?\u12a5",
                             u8"\u12b4",
                             u8"\u12c2\u001d?\u12a5",
                             u8"\u12c2",
                             u8"\u1312\u001d?\u12a5",
                             u8"\u1314",
                             u8"\u2d94\u001d?\u12a5",
                             u8"\u2d95",
                             u8"\u1342\u001d?\u12a5",
                             u8"\u1344",
                             u8"\u1346\u001d?\u12a0",
                             u8"\u1347",
                             u8"\u1381\u001d?\u12a5",
                             u8"\u1382",
                             u8"\u1385\u001d?\u12a5",
                             u8"\u1386",
                             u8"\u138b\u001d?\u12a5",
                             u8"\u138a",
                             u8"\u138f\u001d?\u12a5",
                             u8"\u138e",
                             u8"\u1372\u001d?0",
                             u8"\u137b",
                             u8"\u1201\u001d?\u12a0",
                             u8"\u128b",
                             u8"\u1201\u001d?\u12a1\u001d?",
                             u8"\u128d",
                             u8"\u1201\u001d?\u12a2\u001d?",
                             u8"\u128a",
                             u8"\u1201\u001d?\u12a5",
                             u8"\u1288",
                             u8"\u1201\u001d?\u12a4\u001d?",
                             u8"\u128c",
                             u8"\u1202\u001d?\u12a5",
                             u8"\u1204",
                             u8"\u1206\u001d?\u12a0",
                             u8"\u1207",
                             u8"\u1209\u001d?\u12a0",
                             u8"\u120f",
                             u8"\u120a\u001d?\u12a5",
                             u8"\u120c",
                             u8"\u120e\u001d?\u12a0",
                             u8"\u2d80",
                             u8"\u1211\u001d?\u12a0",
                             u8"\u1217",
                             u8"\u1212\u001d?\u12a5",
                             u8"\u1214",
                             u8"\u1219\u001d?\u12a0",
                             u8"\u121f",
                             u8"\u121a\u001d?\u12a5",
                             u8"\u121c",
                             u8"\u121e\u001d?\u12a0",
                             u8"\u2d81",
                             u8"\u121d\u12ed\u12a0",
                             u8"\u1359",
                             u8"\u1225\u001d?\u12a0",
                             u8"\u1223",
                             u8"\u1225\u001d?\u12a1\u001d?",
                             u8"\u1221",
                             u8"\u1225\u001d?\u12a2\u001d?",
                             u8"\u1222",
                             u8"\u1225\u001d?\u12a4\u001d?",
                             u8"\u1224",
                             u8"\u1225\u001d?\u12a5",
                             u8"\u1220",
                             u8"\u1225\u001d?\u12a6\u001d?",
                             u8"\u1226",
                             u8"\u1229\u001d?\u12a0",
                             u8"\u122f",
                             u8"\u122a\u001d?\u12a5",
                             u8"\u122c",
                             u8"\u122e\u001d?\u12a0",
                             u8"\u2d82",
                             u8"\u122d\u12ed\u12a0",
                             u8"\u1358",
                             u8"\u1231\u001d?\u12a0",
                             u8"\u1237",
                             u8"\u1232\u001d?\u12a5",
                             u8"\u1234",
                             u8"\u1236\u001d?\u12a0",
                             u8"\u2d83",
                             u8"\u1239\u001d?\u12a0",
                             u8"\u123f",
                             u8"\u123a\u001d?\u12a5",
                             u8"\u123c",
                             u8"\u123e\u001d?\u12a0",
                             u8"\u2d84",
                             u8"\u1241\u001d?\u12a0",
                             u8"\u124b",
                             u8"\u1241\u001d?\u12a1\u001d?",
                             u8"\u124d",
                             u8"\u1241\u001d?\u12a2\u001d?",
                             u8"\u124a",
                             u8"\u1241\u001d?\u12a4\u001d?",
                             u8"\u124c",
                             u8"\u1241\u001d?\u12a5",
                             u8"\u1248",
                             u8"\u1242\u001d?\u12a5",
                             u8"\u1244",
                             u8"\u1246\u001d?\u12a0",
                             u8"\u1247",
                             u8"\u1246\u001d?\u12a6\u001d?",
                             u8"\u1248",
                             u8"\u1255\u12a1\u001d?\u12a0",
                             u8"\u125b",
                             u8"\u1255\u12a1\u001d?\u12a2\u001d?",
                             u8"\u125a",
                             u8"\u1255\u12a1\u001d?\u12a4\u001d?",
                             u8"\u125c",
                             u8"\u1255\u12a1\u001d?\u12a5",
                             u8"\u1258",
                             u8"\u1255\u12a1\u001d?\u12a1\u001d?",
                             u8"\u1251",
                             u8"\u1252\u001d?\u12a5",
                             u8"\u1254",
                             u8"\u1261\u001d?\u12a0",
                             u8"\u1267",
                             u8"\u1262\u001d?\u12a5",
                             u8"\u1264",
                             u8"\u1266\u001d?\u12a0",
                             u8"\u2d85",
                             u8"\u1269\u001d?\u12a0",
                             u8"\u126f",
                             u8"\u126a\u001d?\u12a5",
                             u8"\u126c",
                             u8"\u1271\u001d?\u12a0",
                             u8"\u1277",
                             u8"\u1272\u001d?\u12a5",
                             u8"\u1274",
                             u8"\u1276\u001d?\u12a0",
                             u8"\u2d86",
                             u8"\u1279\u001d?\u12a0",
                             u8"\u127f",
                             u8"\u127a\u001d?\u12a5",
                             u8"\u127c",
                             u8"\u127e\u001d?\u12a0",
                             u8"\u2d87",
                             u8"\u1286\u001d?\u12a6\u001d?",
                             u8"\u1288",
                             u8"\u1205\u1205\u12a0",
                             u8"\u1283",
                             u8"\u1205\u1205\u12a1\u001d?",
                             u8"\u1281",
                             u8"\u1205\u1205\u12a2\u001d?",
                             u8"\u1282",
                             u8"\u1205\u1205\u12a4\u001d?",
                             u8"\u1284",
                             u8"\u1205\u1205\u12a5",
                             u8"\u1280",
                             u8"\u1205\u1205\u12a6\u001d?",
                             u8"\u1286",
                             u8"\u1291\u001d?\u12a0",
                             u8"\u1297",
                             u8"\u1292\u001d?\u12a5",
                             u8"\u1294",
                             u8"\u1296\u001d?\u12a0",
                             u8"\u2d88",
                             u8"\u1299\u001d?\u12a0",
                             u8"\u129f",
                             u8"\u129a\u001d?\u12a5",
                             u8"\u129c",
                             u8"\u129e\u001d?\u12a0",
                             u8"\u2d89",
                             u8"\u12a2\u001d?\u12a5",
                             u8"\u12a4",
                             u8"\u12d5\u001d?\u12a5",
                             u8"\u12d0",
                             u8"\u12d5\u001d?\u12a1\u001d?",
                             u8"\u12d1",
                             u8"\u12d5\u001d?\u12a2\u001d?",
                             u8"\u12d2",
                             u8"\u12d5\u001d?\u12a0",
                             u8"\u12d3",
                             u8"\u12d5\u001d?\u12a6\u001d?",
                             u8"\u12d6",
                             u8"\u12d3\u001d?\u12a0",
                             u8"\u12d0",
                             u8"\u12a9\u001d?\u12a0",
                             u8"\u12b3",
                             u8"\u12a9\u001d?\u12a1\u001d?",
                             u8"\u12b5",
                             u8"\u12a9\u001d?\u12a2\u001d?",
                             u8"\u12b2",
                             u8"\u12a9\u001d?\u12a4\u001d?",
                             u8"\u12b4",
                             u8"\u12a9\u001d?\u12a5",
                             u8"\u12b0",
                             u8"\u12aa\u001d?\u12a5",
                             u8"\u12ac",
                             u8"\u12ae\u001d?\u12a0",
                             u8"\u12af",
                             u8"\u12ae\u001d?\u12a6\u001d?",
                             u8"\u12b0",
                             u8"\u12b9\u001d?\u12a0",
                             u8"\u12c3",
                             u8"\u12b9\u001d?\u12a1\u001d?",
                             u8"\u12c5",
                             u8"\u12b9\u001d?\u12a2\u001d?",
                             u8"\u12c2",
                             u8"\u12b9\u001d?\u12a4\u001d?",
                             u8"\u12c4",
                             u8"\u12ba\u001d?\u12a5",
                             u8"\u12bc",
                             u8"\u12be\u001d?\u12a6\u001d?",
                             u8"\u12c0",
                             u8"\u12ca\u001d?\u12a5",
                             u8"\u12cc",
                             u8"\u12ce\u001d?\u12a0",
                             u8"\u12cf",
                             u8"\u12d9\u001d?\u12a0",
                             u8"\u12df",
                             u8"\u12da\u001d?\u12a5",
                             u8"\u12dc",
                             u8"\u12de\u001d?\u12a0",
                             u8"\u2d8b",
                             u8"\u12e1\u001d?\u12a0",
                             u8"\u12e7",
                             u8"\u12e2\u001d?\u12a5",
                             u8"\u12e4",
                             u8"\u12ea\u001d?\u12a5",
                             u8"\u12ec",
                             u8"\u12ee\u001d?\u12a0",
                             u8"\u12ef",
                             u8"\u12f1\u001d?\u12a0",
                             u8"\u12f7",
                             u8"\u12f2\u001d?\u12a5",
                             u8"\u12f4",
                             u8"\u12f6\u001d?\u12a0",
                             u8"\u2d8c",
                             u8"\u12f9\u001d?\u12a0",
                             u8"\u12ff",
                             u8"\u12fa\u001d?\u12a5",
                             u8"\u12fc",
                             u8"\u12fe\u001d?\u12a0",
                             u8"\u2d8d",
                             u8"\u1301\u001d?\u12a0",
                             u8"\u1307",
                             u8"\u1302\u001d?\u12a5",
                             u8"\u1304",
                             u8"\u1306\u001d?\u12a0",
                             u8"\u2d8e",
                             u8"\u1309\u001d?\u12a0",
                             u8"\u1313",
                             u8"\u1309\u001d?\u12a1\u001d?",
                             u8"\u1315",
                             u8"\u1309\u001d?\u12a2\u001d?",
                             u8"\u1312",
                             u8"\u1309\u001d?\u12a4\u001d?",
                             u8"\u1314",
                             u8"\u1309\u001d?\u12a5",
                             u8"\u1310",
                             u8"\u130a\u001d?\u12a5",
                             u8"\u130c",
                             u8"\u130e\u001d?\u12a0",
                             u8"\u130f",
                             u8"\u130e\u001d?\u12a6\u001d?",
                             u8"\u1310",
                             u8"\u1319\u001d?\u12a0",
                             u8"\u131f",
                             u8"\u1319\u001d?\u12a1\u001d?",
                             u8"\u2d96",
                             u8"\u1319\u001d?\u12a2\u001d?",
                             u8"\u2d94",
                             u8"\u1319\u001d?\u12a4\u001d?",
                             u8"\u2d95",
                             u8"\u1319\u001d?\u12a5",
                             u8"\u2d93",
                             u8"\u131a\u001d?\u12a5",
                             u8"\u131c",
                             u8"\u1321\u001d?\u12a0",
                             u8"\u1327",
                             u8"\u1322\u001d?\u12a5",
                             u8"\u1324",
                             u8"\u1326\u001d?\u12a0",
                             u8"\u2d8f",
                             u8"\u1329\u001d?\u12a0",
                             u8"\u132f",
                             u8"\u132e\u001d?\u12a0",
                             u8"\u2d90",
                             u8"\u1331\u001d?\u12a0",
                             u8"\u1337",
                             u8"\u1336\u001d?\u12a0",
                             u8"\u2d91",
                             u8"\u1339\u001d?\u12a0",
                             u8"\u133f",
                             u8"\u133a\u001d?\u12a5",
                             u8"\u133c",
                             u8"\u1345\u001d?\u12a1\u001d?",
                             u8"\u1341",
                             u8"\u1345\u001d?\u12a2\u001d?",
                             u8"\u1342",
                             u8"\u1345\u001d?\u12a4\u001d?",
                             u8"\u1344",
                             u8"\u1345\u001d?\u12a5",
                             u8"\u1340",
                             u8"\u1345\u001d?\u12a6\u001d?",
                             u8"\u1346",
                             u8"\u134d\u12ed\u12a0",
                             u8"\u135a",
                             u8"\u1349\u001d?\u12a0",
                             u8"\u134f",
                             u8"\u134a\u001d?\u12a5",
                             u8"\u134c",
                             u8"\u1351\u001d?\u12a0",
                             u8"\u1357",
                             u8"\u1352\u001d?\u12a5",
                             u8"\u1354",
                             u8"\u1356\u001d?\u12a0",
                             u8"\u2d92",
                             u8"\u2da2\u001d?\u12a5",
                             u8"\u2da4",
                             u8"\u2daa\u001d?\u12a5",
                             u8"\u2dac",
                             u8"\u2db2\u001d?\u12a5",
                             u8"\u2db4",
                             u8"\u2dba\u001d?\u12a5",
                             u8"\u2dbc",
                             u8"\u2dc2\u001d?\u12a5",
                             u8"\u2dc4",
                             u8"\u2dca\u001d?\u12a5",
                             u8"\u2dcc",
                             u8"\u2dd2\u001d?\u12a5",
                             u8"\u2dd4",
                             u8"\u2dda\u001d?\u12a5",
                             u8"\u2ddc",
                             u8"`\u121d\u12a1\u001d?",
                             u8"\u1383",
                             u8"`\u121d\u12a2\u001d?",
                             u8"\u1381",
                             u8"`\u121d\u12a4\u001d?",
                             u8"\u1382",
                             u8"`\u121d\u12a5",
                             u8"\u1380",
                             u8"`\u1265\u12a1\u001d?",
                             u8"\u1387",
                             u8"`\u1265\u12a2\u001d?",
                             u8"\u1385",
                             u8"`\u1265\u12a4\u001d?",
                             u8"\u1386",
                             u8"`\u1265\u12a5",
                             u8"\u1384",
                             u8"`\u134d\u12a1\u001d?",
                             u8"\u1389",
                             u8"`\u134d\u12a2\u001d?",
                             u8"\u138b",
                             u8"`\u134d\u12a4\u001d?",
                             u8"\u138a",
                             u8"`\u134d\u12a5",
                             u8"\u1388",
                             u8"`\u1355\u12a1\u001d?",
                             u8"\u138d",
                             u8"`\u1355\u12a2\u001d?",
                             u8"\u138f",
                             u8"`\u1355\u12a4\u001d?",
                             u8"\u138e",
                             u8"`\u1355\u12a5",
                             u8"\u138c",
                             u8"\u1365\u001d?\u1363",
                             u8",",
                             u8"\u00ab\u001d?\u2039",
                             u8"<",
                             u8"\u00bb\u001d?\u203a",
                             u8">",
                             u8"`\u1361#",
                             u8"\u1368",
                             u8"`\u1361\\+",
                             u8"\u1360",
                             u8"\u1369\u001d?0",
                             u8"\u1372",
                             u8"\u136a\u001d?0",
                             u8"\u1373",
                             u8"\u136b\u001d?0",
                             u8"\u1374",
                             u8"\u136c\u001d?0",
                             u8"\u1375",
                             u8"\u136d\u001d?0",
                             u8"\u1376",
                             u8"\u136e\u001d?0",
                             u8"\u1377",
                             u8"\u136f\u001d?0",
                             u8"\u1378",
                             u8"\u1370\u001d?0",
                             u8"\u1379",
                             u8"\u1371\u001d?0",
                             u8"\u137a",
                             u8"\u1205\u12a0",
                             u8"\u1203",
                             u8"\u1205\u12a1\u001d?",
                             u8"\u1201",
                             u8"\u1205\u12a2\u001d?",
                             u8"\u1202",
                             u8"\u1205\u12a4\u001d?",
                             u8"\u1204",
                             u8"\u1205\u12a5",
                             u8"\u1200",
                             u8"\u1205\u12a6\u001d?",
                             u8"\u1206",
                             u8"\u120d\u12a0",
                             u8"\u120b",
                             u8"\u120d\u12a1\u001d?",
                             u8"\u1209",
                             u8"\u120d\u12a2\u001d?",
                             u8"\u120a",
                             u8"\u120d\u12a4\u001d?",
                             u8"\u120c",
                             u8"\u120d\u12a5",
                             u8"\u1208",
                             u8"\u120d\u12a6\u001d?",
                             u8"\u120e",
                             u8"\u1215\u12a0",
                             u8"\u1213",
                             u8"\u1215\u12a1\u001d?",
                             u8"\u1211",
                             u8"\u1215\u12a2\u001d?",
                             u8"\u1212",
                             u8"\u1215\u12a4\u001d?",
                             u8"\u1214",
                             u8"\u1215\u12a5",
                             u8"\u1210",
                             u8"\u1215\u12a6\u001d?",
                             u8"\u1216",
                             u8"\u121d\u12a0",
                             u8"\u121b",
                             u8"\u121d\u12a1\u001d?",
                             u8"\u1219",
                             u8"\u121d\u12a2\u001d?",
                             u8"\u121a",
                             u8"\u121d\u12a4\u001d?",
                             u8"\u121c",
                             u8"\u121d\u12a5",
                             u8"\u1218",
                             u8"\u121d\u12a6\u001d?",
                             u8"\u121e",
                             u8"\u1235\u1235",
                             u8"\u1225",
                             u8"\u122d\u12a0",
                             u8"\u122b",
                             u8"\u122d\u12a1\u001d?",
                             u8"\u1229",
                             u8"\u122d\u12a2\u001d?",
                             u8"\u122a",
                             u8"\u122d\u12a4\u001d?",
                             u8"\u122c",
                             u8"\u122d\u12a5",
                             u8"\u1228",
                             u8"\u122d\u12a6\u001d?",
                             u8"\u122e",
                             u8"\u1235\u12a0",
                             u8"\u1233",
                             u8"\u1235\u12a1\u001d?",
                             u8"\u1231",
                             u8"\u1235\u12a2\u001d?",
                             u8"\u1232",
                             u8"\u1235\u12a4\u001d?",
                             u8"\u1234",
                             u8"\u1235\u12a5",
                             u8"\u1230",
                             u8"\u1235\u12a6\u001d?",
                             u8"\u1236",
                             u8"\u123d\u12a0",
                             u8"\u123b",
                             u8"\u123d\u12a1\u001d?",
                             u8"\u1239",
                             u8"\u123d\u12a2\u001d?",
                             u8"\u123a",
                             u8"\u123d\u12a4\u001d?",
                             u8"\u123c",
                             u8"\u123d\u12a5",
                             u8"\u1238",
                             u8"\u123d\u12a6\u001d?",
                             u8"\u123e",
                             u8"\u1245\u12a0",
                             u8"\u1243",
                             u8"\u1245\u12a1\u001d?",
                             u8"\u1241",
                             u8"\u1245\u12a2\u001d?",
                             u8"\u1242",
                             u8"\u1245\u12a4\u001d?",
                             u8"\u1244",
                             u8"\u1245\u12a5",
                             u8"\u1240",
                             u8"\u1245\u12a6\u001d?",
                             u8"\u1246",
                             u8"\u1255\u12a0",
                             u8"\u1253",
                             u8"\u1255\u12a2\u001d?",
                             u8"\u1252",
                             u8"\u1255\u12a4\u001d?",
                             u8"\u1254",
                             u8"\u1255\u12a5",
                             u8"\u1250",
                             u8"\u1255\u12a6\u001d?",
                             u8"\u1256",
                             u8"\u1265\u12a0",
                             u8"\u1263",
                             u8"\u1265\u12a1\u001d?",
                             u8"\u1261",
                             u8"\u1265\u12a2\u001d?",
                             u8"\u1262",
                             u8"\u1265\u12a4\u001d?",
                             u8"\u1264",
                             u8"\u1265\u12a5",
                             u8"\u1260",
                             u8"\u1265\u12a6\u001d?",
                             u8"\u1266",
                             u8"\u126d\u12a0",
                             u8"\u126b",
                             u8"\u126d\u12a1\u001d?",
                             u8"\u1269",
                             u8"\u126d\u12a2\u001d?",
                             u8"\u126a",
                             u8"\u126d\u12a4\u001d?",
                             u8"\u126c",
                             u8"\u126d\u12a5",
                             u8"\u1268",
                             u8"\u126d\u12a6\u001d?",
                             u8"\u126e",
                             u8"\u1275\u12a0",
                             u8"\u1273",
                             u8"\u1275\u12a1\u001d?",
                             u8"\u1271",
                             u8"\u1275\u12a2\u001d?",
                             u8"\u1272",
                             u8"\u1275\u12a4\u001d?",
                             u8"\u1274",
                             u8"\u1275\u12a5",
                             u8"\u1270",
                             u8"\u1275\u12a6\u001d?",
                             u8"\u1276",
                             u8"\u127d\u12a0",
                             u8"\u127b",
                             u8"\u127d\u12a1\u001d?",
                             u8"\u1279",
                             u8"\u127d\u12a2\u001d?",
                             u8"\u127a",
                             u8"\u127d\u12a4\u001d?",
                             u8"\u127c",
                             u8"\u127d\u12a5",
                             u8"\u1278",
                             u8"\u127d\u12a6\u001d?",
                             u8"\u127e",
                             u8"\u1285\u12a0",
                             u8"\u1283",
                             u8"\u1285\u12a1\u001d?",
                             u8"\u1281",
                             u8"\u1285\u12a2\u001d?",
                             u8"\u1282",
                             u8"\u1285\u12a4\u001d?",
                             u8"\u1284",
                             u8"\u1285\u12a5",
                             u8"\u1280",
                             u8"\u1285\u12a6\u001d?",
                             u8"\u1286",
                             u8"\u1295\u12a0",
                             u8"\u1293",
                             u8"\u1295\u12a1\u001d?",
                             u8"\u1291",
                             u8"\u1295\u12a2\u001d?",
                             u8"\u1292",
                             u8"\u1295\u12a4\u001d?",
                             u8"\u1294",
                             u8"\u1295\u12a5",
                             u8"\u1290",
                             u8"\u1295\u12a6\u001d?",
                             u8"\u1296",
                             u8"\u129d\u12a0",
                             u8"\u129b",
                             u8"\u129d\u12a1\u001d?",
                             u8"\u1299",
                             u8"\u129d\u12a2\u001d?",
                             u8"\u129a",
                             u8"\u129d\u12a4\u001d?",
                             u8"\u129c",
                             u8"\u129d\u12a5",
                             u8"\u1298",
                             u8"\u129d\u12a6\u001d?",
                             u8"\u129e",
                             u8"\u12a0\u12a1\u001d?",
                             u8"\u12a1",
                             u8"\u12a0\u12a2\u001d?",
                             u8"\u12a2",
                             u8"\u12a0\u12a6\u001d?",
                             u8"\u12a6",
                             u8"\u12d0\u001d?\u12a1\u001d?",
                             u8"\u12d1",
                             u8"\u12d0\u001d?\u12a2\u001d?",
                             u8"\u12d2",
                             u8"\u12d0\u001d?\u12a0",
                             u8"\u12d3",
                             u8"\u12d0\u001d?\u12a6\u001d?",
                             u8"\u12d6",
                             u8"\u12a1\u001d?\u12a1\u001d?",
                             u8"\u12d1",
                             u8"\u12a2\u001d?\u12a2\u001d?",
                             u8"\u12d2",
                             u8"\u12a0\u12a0",
                             u8"\u12d3",
                             u8"\u12a6\u001d?\u12a6\u001d?",
                             u8"\u12d6",
                             u8"\u12a5\u12a1\u001d?",
                             u8"\u12a1",
                             u8"\u12a5\u12a2\u001d?",
                             u8"\u12a2",
                             u8"\u12a5\u12a0",
                             u8"\u12a3",
                             u8"\u12a5\u12a6\u001d?",
                             u8"\u12a6",
                             u8"\u12a5\u12a5",
                             u8"\u12d5",
                             u8"\u12a0\u12a5",
                             u8"\u12a7",
                             u8"\u12ad\u12a0",
                             u8"\u12ab",
                             u8"\u12ad\u12a1\u001d?",
                             u8"\u12a9",
                             u8"\u12ad\u12a2\u001d?",
                             u8"\u12aa",
                             u8"\u12ad\u12a4\u001d?",
                             u8"\u12ac",
                             u8"\u12ad\u12a5",
                             u8"\u12a8",
                             u8"\u12ad\u12a6\u001d?",
                             u8"\u12ae",
                             u8"\u12bd\u12a0",
                             u8"\u12bb",
                             u8"\u12bd\u12a1\u001d?",
                             u8"\u12b9",
                             u8"\u12bd\u12a2\u001d?",
                             u8"\u12ba",
                             u8"\u12bd\u12a4\u001d?",
                             u8"\u12bc",
                             u8"\u12bd\u12a5",
                             u8"\u12b8",
                             u8"\u12bd\u12a6\u001d?",
                             u8"\u12be",
                             u8"\u12cd\u12a0",
                             u8"\u12cb",
                             u8"\u12cd\u12a1\u001d?",
                             u8"\u12c9",
                             u8"\u12cd\u12a2\u001d?",
                             u8"\u12ca",
                             u8"\u12cd\u12a4\u001d?",
                             u8"\u12cc",
                             u8"\u12cd\u12a5",
                             u8"\u12c8",
                             u8"\u12cd\u12a6\u001d?",
                             u8"\u12ce",
                             u8"\u12dd\u12a0",
                             u8"\u12db",
                             u8"\u12dd\u12a1\u001d?",
                             u8"\u12d9",
                             u8"\u12dd\u12a2\u001d?",
                             u8"\u12da",
                             u8"\u12dd\u12a4\u001d?",
                             u8"\u12dc",
                             u8"\u12dd\u12a5",
                             u8"\u12d8",
                             u8"\u12dd\u12a6\u001d?",
                             u8"\u12de",
                             u8"\u12e5\u12a0",
                             u8"\u12e3",
                             u8"\u12e5\u12a1\u001d?",
                             u8"\u12e1",
                             u8"\u12e5\u12a2\u001d?",
                             u8"\u12e2",
                             u8"\u12e5\u12a4\u001d?",
                             u8"\u12e4",
                             u8"\u12e5\u12a5",
                             u8"\u12e0",
                             u8"\u12e5\u12a6\u001d?",
                             u8"\u12e6",
                             u8"\u12ed\u12a0",
                             u8"\u12eb",
                             u8"\u12ed\u12a1\u001d?",
                             u8"\u12e9",
                             u8"\u12ed\u12a2\u001d?",
                             u8"\u12ea",
                             u8"\u12ed\u12a4\u001d?",
                             u8"\u12ec",
                             u8"\u12ed\u12a5",
                             u8"\u12e8",
                             u8"\u12ed\u12a6\u001d?",
                             u8"\u12ee",
                             u8"\u12f5\u12a0",
                             u8"\u12f3",
                             u8"\u12f5\u12a1\u001d?",
                             u8"\u12f1",
                             u8"\u12f5\u12a2\u001d?",
                             u8"\u12f2",
                             u8"\u12f5\u12a4\u001d?",
                             u8"\u12f4",
                             u8"\u12f5\u12a5",
                             u8"\u12f0",
                             u8"\u12f5\u12a6\u001d?",
                             u8"\u12f6",
                             u8"\u12fd\u12a0",
                             u8"\u12fb",
                             u8"\u12fd\u12a1\u001d?",
                             u8"\u12f9",
                             u8"\u12fd\u12a2\u001d?",
                             u8"\u12fa",
                             u8"\u12fd\u12a4\u001d?",
                             u8"\u12fc",
                             u8"\u12fd\u12a5",
                             u8"\u12f8",
                             u8"\u12fd\u12a6\u001d?",
                             u8"\u12fe",
                             u8"\u1305\u12a0",
                             u8"\u1303",
                             u8"\u1305\u12a1\u001d?",
                             u8"\u1301",
                             u8"\u1305\u12a2\u001d?",
                             u8"\u1302",
                             u8"\u1305\u12a4\u001d?",
                             u8"\u1304",
                             u8"\u1305\u12a5",
                             u8"\u1300",
                             u8"\u1305\u12a6\u001d?",
                             u8"\u1306",
                             u8"\u130d\u12a0",
                             u8"\u130b",
                             u8"\u130d\u12a1\u001d?",
                             u8"\u1309",
                             u8"\u130d\u12a2\u001d?",
                             u8"\u130a",
                             u8"\u130d\u12a4\u001d?",
                             u8"\u130c",
                             u8"\u130d\u12a5",
                             u8"\u1308",
                             u8"\u130d\u12a6\u001d?",
                             u8"\u130e",
                             u8"\u131d\u12a0",
                             u8"\u131b",
                             u8"\u131d\u12a1\u001d?",
                             u8"\u1319",
                             u8"\u131d\u12a2\u001d?",
                             u8"\u131a",
                             u8"\u131d\u12a4\u001d?",
                             u8"\u131c",
                             u8"\u131d\u12a5",
                             u8"\u1318",
                             u8"\u131d\u12a6\u001d?",
                             u8"\u131e",
                             u8"\u1325\u12a0",
                             u8"\u1323",
                             u8"\u1325\u12a1\u001d?",
                             u8"\u1321",
                             u8"\u1325\u12a2\u001d?",
                             u8"\u1322",
                             u8"\u1325\u12a4\u001d?",
                             u8"\u1324",
                             u8"\u1325\u12a5",
                             u8"\u1320",
                             u8"\u1325\u12a6\u001d?",
                             u8"\u1326",
                             u8"\u132d\u12a0",
                             u8"\u132b",
                             u8"\u132d\u12a1\u001d?",
                             u8"\u1329",
                             u8"\u132d\u12a2\u001d?",
                             u8"\u132a",
                             u8"\u132d\u12a4\u001d?",
                             u8"\u132c",
                             u8"\u132d\u12a5",
                             u8"\u1328",
                             u8"\u132d\u12a6\u001d?",
                             u8"\u132e",
                             u8"\u1335\u12a0",
                             u8"\u1333",
                             u8"\u1335\u12a1\u001d?",
                             u8"\u1331",
                             u8"\u1335\u12a2\u001d?",
                             u8"\u1332",
                             u8"\u1335\u12a4\u001d?",
                             u8"\u1334",
                             u8"\u1335\u12a5",
                             u8"\u1330",
                             u8"\u1335\u12a6\u001d?",
                             u8"\u1336",
                             u8"\u133d\u12a0",
                             u8"\u133b",
                             u8"\u133d\u12a1\u001d?",
                             u8"\u1339",
                             u8"\u133d\u12a2\u001d?",
                             u8"\u133a",
                             u8"\u133d\u12a4\u001d?",
                             u8"\u133c",
                             u8"\u133d\u12a5",
                             u8"\u1338",
                             u8"\u133d\u12a6\u001d?",
                             u8"\u133e",
                             u8"\u1345\u001d?\u12a0",
                             u8"\u1343",
                             u8"\u133d\u133d",
                             u8"\u1345",
                             u8"\u134d\u12a0",
                             u8"\u134b",
                             u8"\u134d\u12a1\u001d?",
                             u8"\u1349",
                             u8"\u134d\u12a2\u001d?",
                             u8"\u134a",
                             u8"\u134d\u12a4\u001d?",
                             u8"\u134c",
                             u8"\u134d\u12a5",
                             u8"\u1348",
                             u8"\u134d\u12a6\u001d?",
                             u8"\u134e",
                             u8"\u1355\u12a0",
                             u8"\u1353",
                             u8"\u1355\u12a1\u001d?",
                             u8"\u1351",
                             u8"\u1355\u12a2\u001d?",
                             u8"\u1352",
                             u8"\u1355\u12a4\u001d?",
                             u8"\u1354",
                             u8"\u1355\u12a5",
                             u8"\u1350",
                             u8"\u1355\u12a6\u001d?",
                             u8"\u1356",
                             u8"\u2da5\u12a0",
                             u8"\u2da3",
                             u8"\u2da5\u12a1\u001d?",
                             u8"\u2da1",
                             u8"\u2da5\u12a2\u001d?",
                             u8"\u2da2",
                             u8"\u2da5\u12a4\u001d?",
                             u8"\u2da4",
                             u8"\u2da5\u12a5",
                             u8"\u2da0",
                             u8"\u2da5\u12a6\u001d?",
                             u8"\u2da6",
                             u8"\u2dad\u12a0",
                             u8"\u2dab",
                             u8"\u2dad\u12a1\u001d?",
                             u8"\u2da9",
                             u8"\u2dad\u12a2\u001d?",
                             u8"\u2daa",
                             u8"\u2dad\u12a4\u001d?",
                             u8"\u2dac",
                             u8"\u2dad\u12a5",
                             u8"\u2da8",
                             u8"\u2dad\u12a6\u001d?",
                             u8"\u2dae",
                             u8"\u2db5\u12a0",
                             u8"\u2db3",
                             u8"\u2db5\u12a1\u001d?",
                             u8"\u2db1",
                             u8"\u2db5\u12a2\u001d?",
                             u8"\u2db2",
                             u8"\u2db5\u12a4\u001d?",
                             u8"\u2db4",
                             u8"\u2db5\u12a5",
                             u8"\u2db0",
                             u8"\u2db5\u12a6\u001d?",
                             u8"\u2db6",
                             u8"\u2dbd\u12a0",
                             u8"\u2dbb",
                             u8"\u2dbd\u12a1\u001d?",
                             u8"\u2db9",
                             u8"\u2dbd\u12a2\u001d?",
                             u8"\u2dba",
                             u8"\u2dbd\u12a4\u001d?",
                             u8"\u2dbc",
                             u8"\u2dbd\u12a5",
                             u8"\u2db8",
                             u8"\u2dbd\u12a6\u001d?",
                             u8"\u2dbe",
                             u8"\u2dc5\u12a0",
                             u8"\u2dc3",
                             u8"\u2dc5\u12a1\u001d?",
                             u8"\u2dc1",
                             u8"\u2dc5\u12a2\u001d?",
                             u8"\u2dc2",
                             u8"\u2dc5\u12a4\u001d?",
                             u8"\u2dc4",
                             u8"\u2dc5\u12a5",
                             u8"\u2dc0",
                             u8"\u2dc5\u12a6\u001d?",
                             u8"\u2dc6",
                             u8"\u2dcd\u12a0",
                             u8"\u2dcb",
                             u8"\u2dcd\u12a1\u001d?",
                             u8"\u2dc9",
                             u8"\u2dcd\u12a2\u001d?",
                             u8"\u2dca",
                             u8"\u2dcd\u12a4\u001d?",
                             u8"\u2dcc",
                             u8"\u2dcd\u12a5",
                             u8"\u2dc8",
                             u8"\u2dcd\u12a6\u001d?",
                             u8"\u2dce",
                             u8"\u2dd5\u12a0",
                             u8"\u2dd3",
                             u8"\u2dd5\u12a1\u001d?",
                             u8"\u2dd1",
                             u8"\u2dd5\u12a2\u001d?",
                             u8"\u2dd2",
                             u8"\u2dd5\u12a4\u001d?",
                             u8"\u2dd4",
                             u8"\u2dd5\u12a5",
                             u8"\u2dd0",
                             u8"\u2dd5\u12a6\u001d?",
                             u8"\u2dd6",
                             u8"\u2ddd\u12a0",
                             u8"\u2ddb",
                             u8"\u2ddd\u12a1\u001d?",
                             u8"\u2dd9",
                             u8"\u2ddd\u12a2\u001d?",
                             u8"\u2dda",
                             u8"\u2ddd\u12a4\u001d?",
                             u8"\u2ddc",
                             u8"\u2ddd\u12a5",
                             u8"\u2dd8",
                             u8"\u2ddd\u12a6\u001d?",
                             u8"\u2dde",
                             u8"_\u001d?0",
                             u8"\u1399",
                             u8"_\u001d?2",
                             u8"\u1391",
                             u8"_\u001d?3",
                             u8"\u1392",
                             u8"_\u001d?4",
                             u8"\u1393",
                             u8"_\u001d?5",
                             u8"\u1394",
                             u8"_\u001d?6",
                             u8"\u1395",
                             u8"_\u001d?7",
                             u8"\u1396",
                             u8"_\u001d?8",
                             u8"\u1397",
                             u8"_\u001d?9",
                             u8"\u1398",
                             u8"_\u001d?_\u001d?",
                             u8"_",
                             u8"'!",
                             u8"\u00a1",
                             u8"\u1363\u1363",
                             u8"\u1365",
                             u8":-",
                             u8"\u1366",
                             u8"\u1361\u1361",
                             u8"\u1362",
                             u8"\u1364\u1364",
                             u8";",
                             u8"\u2039\u2039",
                             u8"\u00ab",
                             u8"\u203a\u203a",
                             u8"\u00bb",
                             u8"\u1361-",
                             u8"\u1366",
                             u8"`\\?",
                             u8"\u1367",
                             u8"'1",
                             u8"\u1369",
                             u8"'2",
                             u8"\u136a",
                             u8"'3",
                             u8"\u136b",
                             u8"'4",
                             u8"\u136c",
                             u8"'5",
                             u8"\u136d",
                             u8"'6",
                             u8"\u136e",
                             u8"'7",
                             u8"\u136f",
                             u8"'8",
                             u8"\u1370",
                             u8"'9",
                             u8"\u1371"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace ethi
