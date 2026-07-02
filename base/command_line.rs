// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::ops::Deref;

#[cxx::bridge(namespace = "base::rust")]
mod ffi {
    unsafe extern "C++" {
        #[namespace = "base"]
        type CommandLine;

        include!("base/command_line.h");
        include!("base/command_line_rust_shim.h");

        #[rust_name = "initialized_for_current_process"]
        #[Self = "CommandLine"]
        fn InitializedForCurrentProcess() -> bool;
        // This returns a pointer to the global base::CommandLine singleton,
        // which is initialized during early process startup and remains valid
        // for the entire lifetime of the process. It is safe to use in a
        // *read-only* fashion.
        #[rust_name = "for_current_process"]
        #[Self = "CommandLine"]
        fn ForCurrentProcess() -> *mut CommandLine;
        fn NewForProgram(program: &str) -> UniquePtr<CommandLine>;

        fn HasSwitch(cmd: &CommandLine, switch_string: &str) -> bool;
        fn GetSwitchValueASCII(cmd: &CommandLine, switch_string: &str) -> String;
        fn AppendSwitch(cmd: Pin<&mut CommandLine>, switch_string: &str);
        fn AppendSwitchASCII(cmd: Pin<&mut CommandLine>, switch_string: &str, value: &str);
        fn RemoveSwitch(cmd: Pin<&mut CommandLine>, switch_string: &str);
        fn AppendArg(cmd: Pin<&mut CommandLine>, value: &str);
        fn GetArgs(cmd: &CommandLine) -> Vec<String>;
        fn GetProgram(cmd: &CommandLine) -> String;
        fn GetAppOutput(cmd: &CommandLine, output: &mut String) -> bool;
    }
}

/// A read-only wrapper representing the current process's global command line.
///
/// This type is safe to share across threads (`Send` and `Sync`) because it
/// does not allow mutation.
pub struct CurrentCommandLine(*const ffi::CommandLine);

// SAFETY: Because CurrentCommandLine is strictly read-only on the Rust side,
// it is fine to send/share across threads.
unsafe impl Send for CurrentCommandLine {}

// SAFETY: As above
unsafe impl Sync for CurrentCommandLine {}

impl CurrentCommandLine {
    /// Returns the global command line singleton for the current process.
    pub fn get() -> Self {
        Self(ffi::CommandLine::for_current_process())
    }

    /// Returns true if the global CommandLine singleton has been initialized.
    pub fn initialized_for_current_process() -> bool {
        ffi::CommandLine::initialized_for_current_process()
    }
}

impl Deref for CurrentCommandLine {
    type Target = ffi::CommandLine;
    fn deref(&self) -> &Self::Target {
        // SAFETY: This is a pointer to a static variable; we can assume it
        // will always be valid.
        unsafe { &*self.0 }
    }
}

/// A mutable, owned command line instance, typically used for constructing
/// arguments to launch a subprocess.
pub struct SubprocessCommandLine(cxx::UniquePtr<ffi::CommandLine>);

impl SubprocessCommandLine {
    /// Creates a new, independent CommandLine object for the specified program.
    pub fn new(program: &str) -> Self {
        Self(ffi::NewForProgram(program))
    }

    /// Appends a switch (with no value) to the command line.
    pub fn append_switch(&mut self, switch_string: &str) {
        ffi::AppendSwitch(self.0.pin_mut(), switch_string);
    }

    /// Appends a switch with an ASCII value to the command line.
    pub fn append_switch_ascii(&mut self, switch_string: &str, value: &str) {
        ffi::AppendSwitchASCII(self.0.pin_mut(), switch_string, value);
    }

    /// Removes the switch from the command line.
    pub fn remove_switch(&mut self, switch_string: &str) {
        ffi::RemoveSwitch(self.0.pin_mut(), switch_string);
    }

    /// Appends a positional argument to the command line.
    pub fn append_arg(&mut self, arg: &str) {
        ffi::AppendArg(self.0.pin_mut(), arg);
    }

    /// Executes the command line as a subprocess and returns its stdout.
    pub fn get_app_output(&self) -> Option<String> {
        let mut output = String::new();
        let success = ffi::GetAppOutput(&self.0, &mut output);
        if success {
            Some(output)
        } else {
            None
        }
    }
}

impl Deref for SubprocessCommandLine {
    type Target = ffi::CommandLine;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

// Implement shared methods on the opaque C++ type, making them available to
// both CurrentCommandLine and SubprocessCommandLine via Deref.
// TODO(flowerhack, crbug.com/528378185): If-and-when CXX understands
// string_view, this function (and other functions on CommandLine) should
// be auto-generate-able via something like `fn HasSwitch(&self, sv).
impl ffi::CommandLine {
    /// Returns true if the command line has the specified switch.
    pub fn has_switch(&self, switch_string: &str) -> bool {
        ffi::HasSwitch(self, switch_string)
    }

    /// Returns the ASCII value of the specified switch.
    pub fn get_switch_value_ascii(&self, switch_string: &str) -> String {
        ffi::GetSwitchValueASCII(self, switch_string)
    }

    /// Returns the positional arguments.
    pub fn get_args(&self) -> Vec<String> {
        ffi::GetArgs(self)
    }

    /// Returns the program/executable path.
    pub fn get_program(&self) -> String {
        ffi::GetProgram(self)
    }
}
