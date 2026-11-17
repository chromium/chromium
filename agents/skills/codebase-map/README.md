# codebase-map skill

Generates an interactive, self-contained HTML map of a Chromium directory: tree
grouped by file role, resolved OWNERS, DIR_METADATA, recent git activity, who
actually reviews there, and a reviewer planner for a CL.

See [SKILL.md](SKILL.md) for the agent workflow. For humans:

```sh
# From src/: deterministic map, no agent prose, served on localhost.
python3 agents/skills/codebase-map/scripts/codebase_map.py build chrome/browser/ui/tabs --serve
```

Output lands in `out/codebase_map/<dir>.html` (gitignored), with each `/` in the
directory replaced by `__` (`chrome__browser__ui__tabs.html`).

## Layout

```
codebase-map/
  SKILL.md                      agent instructions
  scripts/codebase_map.py       extract / targets / annotate / render / build / serve
  scripts/codebase_map_test.py  unit tests (run by agents/skills/PRESUBMIT.py)
  scripts/fake_dom_for_test.js  minimal DOM the tests run the viewer against
  templates/map.html            the viewer; JSON is inlined at render time
```

`extract` produces facts only. `targets` lists what an agent should describe
(every directory + the N most-changed files). `annotate` merges the agent's
`{summary, highlights, descriptions}` back in. `render` inlines the JSON into
the template. The viewer has no external dependencies, respects
`prefers-color-scheme`, and supports deep links (`#sel=…&pick=…&tab=…&q=…`).

## Design notes

- Owner faces appear in the tree only where the owner set changes from the
  parent, so a face means "OWNERS boundary or per-file rule here".
- Per-file owners are the *nearest* approvers (per-file rules + closest OWNERS
  listing people). The full inheritance chain is shown separately. Directories
  use the same "closest OWNERS listing people" rule.
- `per-file` globs follow Gerrit code-owners' `find-owners` syntax: matched
  against the path relative to the OWNERS file with an implicit `{**/,}` prefix,
  so they also cover subdirectories.
- The tree is built from `git ls-files`, so untracked and generated files are
  excluded; symlinks and submodules below the mapped directory are skipped. A
  directory *inside* a submodule (`v8/src`) is read with the submodule's history
  but mapped against the outermost superproject, so paths, OWNERS above the
  submodule, codesearch links and the output directory are Chromium's.
- OWNERS `include` and code-owners `file:` lines are resolved like `file://`
  (owner lines only); unrecognised directives never become owners, and an
  include that can't be read is kept as `unresolved` and flagged instead of
  being offered as a reviewer. `#{LAST_RESORT_SUGGESTION}` is tracked per OWNERS
  entry: the planner counts such an owner for a file only when nobody else can
  approve it, while the same person stays a regular owner wherever a nearer
  OWNERS lists them without the tag.
- A file's `owners_chain` holds only OWNERS files whose owners can approve it,
  so a matching `per-file ... set noparent` rule ends the chain and nobody is
  shown as an "owner via" a parent.
- DIR_METADATA is a small textproto reader (one-line blocks included) with
  one-level `mixins` and `dirmd`-style inheritance from parent directories. The
  displayed component prefers `buganizer_public`.
- Directory commit counts are distinct commits touching the subtree, not a sum
  over files.
- Reviewers come from `Reviewed-by:` trailers, so "reviews" are approvals that
  landed. The planner weights reviews 2× commits when tie-breaking.
- The reviewer planner is a greedy set cover over picked files (small, not
  guaranteed minimal). Files with identical approver sets form a "pick one of"
  group; clicking an owner in a group pins them and the cover is re-planned
  around that pin. The planner, deep-link path resolution, owner status, "watch
  out" flags and checkbox counts live in the template's `map-lib` script block,
  which the tests run under Node. The page script itself (tree, filter, picking,
  deep links, tabs) is tested the same way against `fake_dom_for_test.js`, a
  small in-memory DOM that throws on anything it doesn't implement.
- The tree builds a directory's rows on first open, checkbox state is computed
  from per-directory picked counts, and the filter is debounced, so large
  directories stay responsive.
- `serve` exposes only the `.html` maps in its directory, checks the `Host`
  header, reuses an already-running server for the same directory, and exits
  after an idle timeout. It binds 127.0.0.1 unless `--proxy` is given, which
  binds all interfaces and also accepts the machine's own host name and any
  `--allow-host`, for browser-side proxies like
  `http://<cloudtop>.c.googlers.com:<port>/`. No proxy domain is trusted by
  default; SKILL.md says which one to allow on a Google cloudtop.

## Testing

```sh
python3 agents/skills/codebase-map/scripts/codebase_map_test.py
```

## Follow-ups

- Dependency view: `#include` and GN `deps` edges between subdirectories are
  cheap to extract from `gn desc` / a grep pass and would fit as a fourth tab.
- Per-file descriptions beyond the hot-N list, on request.
- Cache `extract` output keyed on `HEAD` so re-renders are instant.
