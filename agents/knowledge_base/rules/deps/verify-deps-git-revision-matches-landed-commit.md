---
name: deps/verify-deps-git-revision-matches-landed-commit
rubric: correctness
severity: should_fix
---

**Rule.** When updating, pinning, or rolling repository dependencies in DEPS
(and corresponding Git submodule gitlinks), verify that the revision hash
corresponds to the final landed commit SHA in the upstream or target repository.
Do not use local commit hashes, pre-merge patchset SHAs, or unlanded branch
heads.

**Why.** Pinning an unlanded, pre-merge, or local commit hash causes gclient
sync and bot checkouts to fail when fetching the revision from the target
repository, or causes builds to run against untested or unreviewed code
versions.

## Examples

Bad:

```cpp
# Bad: Pinning a local or pre-landing Gerrit patchset hash that does not exist in upstream main.
vars = {
  'autofill_regex_patterns_revision': '899c9355aa3d4ca57e03357a913f80cb68c6b49f',
}
deps = {
  'src/components/autofill/core/browser/form_parsing/internal_resources': {
      'url': Var('chrome_git') + '/chrome/components/autofill_regex_patterns.git' + '@' +
        Var('autofill_regex_patterns_revision'),
      'condition': 'checkout_src_internal',
  },
}
```

Good:

```cpp
# Good: Pinned revision matches the final landed commit SHA in the upstream repository.
vars = {
  'autofill_regex_patterns_revision': 'ca9699d49255fc461f11dbddfed913dae42852ee',
}
deps = {
  'src/components/autofill/core/browser/form_parsing/internal_resources': {
      'url': Var('chrome_git') + '/chrome/components/autofill_regex_patterns.git' + '@' +
        Var('autofill_regex_patterns_revision'),
      'condition': 'checkout_src_internal',
  },
}
```
