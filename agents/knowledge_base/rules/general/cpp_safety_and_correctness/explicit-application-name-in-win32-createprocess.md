---
name: general/explicit-application-name-in-win32-createprocess
rubric: safety
severity: should_fix
---

**Rule.** When calling Win32 process creation APIs (such as CreateProcessW,
CreateProcessAsUserW, CreateProcessWithTokenW, or CreateProcessWithLogonW),
explicitly pass the trusted executable path in lpApplicationName rather than
passing nullptr. If constructing the command line with arguments, supply the
executable path separately for lpApplicationName.

**Why.** Passing nullptr for lpApplicationName forces Windows to deduce the
executable module path from lpCommandLine by searching space-delimited prefixes.
If the executable path or arguments contain unquoted whitespace or ambiguous
characters, command line parsing heuristics can resolve to an unintended binary,
creating path interception or elevation-of-privilege vulnerabilities.

## Examples

Bad:

```cpp
// Passing nullptr causes Windows to parse lpCommandLine using whitespace heuristics.
if (!::CreateProcessAsUserW(
        primary_token->get(), nullptr, std::data(writeable_command_line),
        /*lpProcessAttributes=*/nullptr, /*lpThreadAttributes=*/nullptr,
        /*bInheritHandles=*/FALSE, /*dwCreationFlags=*/creation_flags,
        /*lpEnvironment=*/env, /*lpCurrentDirectory=*/nullptr,
        startup_information.startup_info(), &temp_process_info)) {
  return false;
}
```

Good:

```cpp
// Explicitly providing lpApplicationName prevents command-line parsing confusion.
if (!::CreateProcessAsUserW(
        primary_token->get(),
        std::data(trusted_command_line->GetProgram().value()),
        std::data(writeable_command_line),
        /*lpProcessAttributes=*/nullptr, /*lpThreadAttributes=*/nullptr,
        /*bInheritHandles=*/FALSE, /*dwCreationFlags=*/creation_flags,
        /*lpEnvironment=*/env, /*lpCurrentDirectory=*/nullptr,
        startup_information.startup_info(), &temp_process_info)) {
  return false;
}
```
