---
name: general/document-parent-of-transitive-dependencies-in-manifests
rubric: readability
severity: nit
---

**Rule.** When explicitly declaring a transitive or indirect dependency in a
dependency manifest (such as .vpython3, vpython.toml, DEPS, or
requirements.txt), add a comment documenting the parent package, tool, or script
that requires it. Precede indirect dependency declarations with explanatory
comments (e.g., '# Dependency of <package>' or '# Used by <tool>') to make their
purpose clear.

**Why.** Transitive and indirect dependencies lack direct call sites in
repository code, making their presence non-obvious to reviewers and future
maintainers. Documenting the parent dependency or consuming tool prevents
confusion and avoids accidental removal as dead code during manifest cleanups or
rolls.

## Examples

Bad:

```cpp
wheel: <
  name: "infra/python/wheels/pexpect/${vpython_platform}"
  version: "version:4.8.0.chromium.1"
>
wheel: <
  name: "infra/python/wheels/ptyprocess-py2_py3"
  version: "version:0.7.0"
>
```

Good:

```cpp
wheel: <
  name: "infra/python/wheels/pexpect/${vpython_platform}"
  version: "version:4.8.0.chromium.1"
>
# Dependency of pexpect.
wheel: <
  name: "infra/python/wheels/ptyprocess-py2_py3"
  version: "version:0.7.0"
>
```
