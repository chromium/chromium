// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! To test that CFG is working, build this executable on Windows and run it
//! as:
//!
//! `out\Release\cdb\cdb.exe -G -g -o .\out\Release\test_control_flow_guard.exe`
//!
//! Which should print:
//! ```
//! (a2d4.bcd8): Security check failure or stack buffer overrun - code c0000409
//!     (!!! second chance !!!)
//! Subcode: 0xa FAST_FAIL_GUARD_ICALL_CHECK_FAILURE
//! ```
//!
//! If cdb.exe is not present, first run `ninja -C out\Release cdb\cdb.exe`.

use std::arch::asm;

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
const NOP_INSTRUCTION_SIZE: usize = 1;
#[cfg(target_arch = "aarch64")]
const NOP_INSTRUCTION_SIZE: usize = 4;

#[inline(never)]
fn nop_sled() {
    unsafe { asm!("nop", "nop", "ret",) }
}

#[inline(never)]
fn indirect_call(func: fn()) {
    func();
}

fn main() {
    let fptr =
        unsafe { std::mem::transmute::<usize, fn()>(nop_sled as usize + NOP_INSTRUCTION_SIZE) };
    // Generates a FAST_FAIL_GUARD_ICALL_CHECK_FAILURE if CFG triggers.
    indirect_call(fptr);
    // Should only reach here if CFG is disabled.
    eprintln!("failed");
}
