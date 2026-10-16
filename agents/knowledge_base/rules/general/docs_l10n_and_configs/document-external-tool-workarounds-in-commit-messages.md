---
name: general/document-external-tool-workarounds-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** When authoring a CL that works around, disables, or resolves a build,
linker, compiler, upstream tool, or external dependency issue (such as symbol
conflicts, tool bugs, or temporarily ignored directories/configurations),
explicitly describe the failure mechanism and resolution in the commit message
body. Include relevant error output, build logs, or links to upstream tracking
issues in both the commit message and accompanying documentation or README
notes, rather than vaguely citing a build failure or leaving the workaround
untracked.

**Why.** Vague commit messages and undocumented workarounds mislead reviewers,
hide context, and obscure why replacing runtime checks or disabling
configurations was necessary. Providing detailed failure context, error logs,
and links to upstream tracking issues in commit messages and documentation
enables reviewers and future maintainers to evaluate the fix and track when the
workaround or warning note should be retired.

## Examples

Bad:

```cpp
// Example 1: Vague build error / implying runtime exceptions for link issues
AddressPoolManager uses bitset::set and bitset::test, which can
cause __throw_out_of_range. This caused a build error in mac-official.
This CL replaces them with index operator.

// Example 2: Missing upstream reference and error details for tool bug
protoc can only generate a descriptor file from a single .proto file.
As a workaround, pass it a wrapper importing all protos.

// Example 3: Disabling folder/feature without linking upstream tracking issue
# gemini-cli policies
This folder is currently not read by gemini CLI...
```

Good:

```cpp
// Example 1: Explicit linker mechanism and resolution in CL description
AddressPoolManager uses bitset::set, which invokes
std::__throw_out_of_range. This introduces an unexpected dependency in
address_pool_manager.o, causing a duplicate symbol link error in
breakpad_unittests. Switching to bitset::operator[] uses assertion
checks instead, eliminating the dependency on std::__throw_out_of_range.

// Example 2: Upstream link and error output in CL description
protoc fails with "Can only process one input file when using
--dependency_out" when given multiple files. As documented in
https://groups.google.com/g/protobuf/c/CuYXum4TBwI/m/wtkUT-6EBgAJ,
this is a known limitation worked around by generating a wrapper proto.

// Example 3: Linked upstream tracking bug in README and CL description
NOTE: This entire directory is currently ignored by gemini CLI due to
an upstream regression. See https://github.com/google-gemini/gemini-cli/issues/18186.
Do not place policy files here until this upstream issue is resolved.
```
