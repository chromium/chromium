---
name: general/document-command-line-switch-terminator-when-forwarding-args
rubric: readability
severity: nit
---

**Rule.** When inserting or appending the '--' switch terminator (via AppendArg,
AppendArgNative, or AppendArgPath) before forwarding positional arguments
(CommandLine::GetArgs()) to a child process's base::CommandLine, add an
explanatory comment. Clarify that '--' is required because GetArgs() strips the
delimiter and subsequent arguments (which might start with '-', '--', or '/')
must not be parsed as switches.

**Why.** Because base::CommandLine::GetArgs() strips the '--' switch terminator
during argument parsing, forwarding arguments directly to a new CommandLine
without inserting '--' allows untrusted positional arguments starting with '-',
'--', or '/' to be misinterpreted as switches. Documenting this parsing detail
prevents future maintainers from removing the seemingly redundant '--' argument.

## Examples

Bad:

```cpp
if (!untrusted_command_line.GetArgs().empty()) {
  trusted_command_line->AppendArgNative(L"--");
  for (const auto& arg : untrusted_command_line.GetArgs()) {
    trusted_command_line->AppendArgNative(arg);
  }
}
```

Good:

```cpp
if (!untrusted_command_line.GetArgs().empty()) {
  // Positional arguments can follow '--' to distinguish them from switches.
  // base::CommandLine strips '--' when parsing, so explicitly append it to
  // ensure the child process does not parse leading '-' or '/' args as switches.
  trusted_command_line->AppendArgNative(L"--");
  for (const auto& arg : untrusted_command_line.GetArgs()) {
    trusted_command_line->AppendArgNative(arg);
  }
}
```
