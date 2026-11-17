---
name: codebase-map
description: Build an interactive, self-contained HTML map of a Chromium directory — file tree grouped by role, resolved OWNERS (inheritance, per-file, set noparent), DIR_METADATA, recent git activity, who actually reviews there, and a reviewer planner for a CL. Use when the user asks to "map", "explain the layout of", "who owns / who should review", or "give me an overview of" a directory, or when onboarding to unfamiliar code.
---

# Codebase map

Produces `out/codebase_map/<dir>.html`: a single file, no external dependencies,
which you then serve on localhost and link for the user (step 5 — always). It
shows:

- **Tree** — directories first, then files grouped by role (public API, headers,
  impl, tests, build, docs, metadata), with LOC, churn bars, and owner faces
  only where the owner set *changes* from the parent.
- **Details** — description, README/header-comment excerpt, DIR_METADATA, who
  can approve (nearest owners, full inheritance chain, per-file rules), and
  recently-active people.
- **Reviewers** — who actually reviews in this directory (from `Reviewed-by:`
  trailers), plus a CL planner: tick files, get a small suggested approver set
  (greedy, not guaranteed minimal), per-file "pick one of" groups to swap
  suggestions, `set noparent` warnings, and a ready `R=` line.
- **Activity** — top contributors (owner-badged), hot-spot files, recent commits
  linking to crrev.

Everything except the prose is extracted deterministically by
`scripts/codebase_map.py`. Your job as the agent is the prose.

## Workflow

Run all commands from the Chromium `src/` root. `$S` below is
`agents/skills/codebase-map/scripts/codebase_map.py`, and `$M` is a
per-directory prefix under the gitignored `out/codebase_map/`, e.g.
`out/codebase_map/chrome__browser__ui__tabs` (the directory with each `/`
replaced by `__`, the same name `render` uses). Don't use fixed `/tmp` names:
concurrent sessions would overwrite each other.

### 1. Extract facts

```sh
python3 $S extract chrome/browser/ui/tabs -o $M.json
```

Takes ~5s for a few hundred files. Use `--since 1.year` (any `git log --since`
value) if the user asks for a different activity window; the default is 6
months.

### 2. Get your annotation targets

```sh
python3 $S targets $M.json -o $M.targets.json
```

This lists every directory plus the 15 most-changed files (`--hot N` to change),
each with the context you need: existing header comment, README excerpt, child
names, DIR_METADATA, and recent commit subjects. **Do not re-read every file in
the directory** — the targets file is designed to be enough. Read a source file
only when a target has no doc comment and the commit subjects don't make its
purpose obvious.

### 3. Write annotations

Create `$M.annotations.json`:

```json
{
  "summary": "2-3 sentences: what this directory is for and how its pieces fit together.",
  "highlights": [
    "3-5 bullets: the entry-point class, the main abstraction, a gotcha, where tests live, ..."
  ],
  "descriptions": {
    "chrome/browser/ui/tabs": "One sentence per target path.",
    "chrome/browser/ui/tabs/tab_strip_model.cc": "..."
  }
}
```

Guidelines for the prose:

- **One sentence** per node, present tense, no "This file…" preamble. Good:
  `Ordered container of tabs; every tab mutation goes through here.`
- Prefer *role in the system* over restating the filename.
- For directories, say what's inside and what it's for; name the key class.
- The header comment (`doc`) is often already a fine description — reuse it,
  tightened, rather than inventing a new one.
- `summary` should let a newcomer decide in 10 seconds whether this is the
  directory they're looking for.
- `highlights` are where you put the non-obvious: "all tab state is tab-scoped
  via `TabFeatures`", "`saved_tab_groups/` has its own OWNERS and
  `set noparent`", "most CLs here are reviewed by X even though Y is also an
  owner".

### 4. Merge and render

```sh
python3 $S annotate $M.json $M.annotations.json
python3 $S render $M.json
```

`render` writes `out/codebase_map/chrome__browser__ui__tabs.html` (gitignored)
and prints the path. Unknown paths and wrongly typed entries in the annotations
are warned about and skipped, so one mistake doesn't lose the batch. The
rendered HTML does not include your local checkout path.

### 5. Serve it and give the user the link (always)

Always start the localhost server, even on a local machine: a `file://` link is
unreliable from IDE chat panels and doesn't work at all on a cloudtop.

```sh
python3 $S serve out/codebase_map/<name>.html --no-open
```

Run it **in the background**. It prints `http://localhost:8765/<name>.html`
(falls back to a free port if 8765 is busy). Remote IDEs auto-detect the URL and
offer to forward the port.

The server only serves the `.html` maps in that directory (nothing else beside
them), refuses requests whose `Host` isn't `localhost`/`127.0.0.1`/`[::1]`, and
exits on its own after 120 minutes without a request (`--idle-timeout MIN`, 0 =
never). If a codebase-map server is already serving `out/codebase_map/` on 8765
in the same mode, `serve` prints the URL and exits immediately instead of
starting another one, so it's safe to run every time.

**Browser-side proxy (`--proxy`).** By default the server only listens on
127.0.0.1, which works from the same machine, through IDE port forwarding, or
through `ssh -L`. It does *not* work through a proxy URL such as
`http://<cloudtop>.c.googlers.com:8765/...` (the proxy reports "connection
refused"). If the user opens maps that way, or the localhost link didn't work
for them and they aren't using `ssh -L`, re-run `serve` with `--proxy`. It then
listens on all interfaces, accepts the machine's own host name, and prints the
proxy URL first. Anyone who can reach the port can read the maps while it runs,
so only use it when asked or when localhost failed, and say so in your reply.

No other host name is trusted unless you name it. Some proxies forward a
different `Host` than the one in the URL; the page then answers 403 and the
script prints the `Host` it refused. Pass that name with `--allow-host <name>`,
or `--allow-host .<domain>` for every name under a domain. The Google cloudtop
web proxy redirects to per-machine names, so there use
`--proxy --allow-host .proxy.googlers.com` from the start.

Your reply to the user must include:

1. The first URL `serve` printed, as the first line: the proxy URL in `--proxy`
   mode, otherwise `http://localhost:<port>/...`.
2. If the session is remote (the script prints a "remote session" notice): the
   `ssh -L` command it printed, for people not in an IDE.
3. A note that the server is running, stops by itself after two idle hours, and
   that you'll stop it sooner on request (kill the background task).
4. The `summary` and `highlights` you wrote, in a few lines of chat.
5. Deep links where useful. The page supports `#sel=<path>`,
   `#pick=<path,path>&tab=reviewers`, `#tab=activity`, `#q=<filter>`, and
   `#hot=<path>&tab=activity` (opens that file's hot-spot row). Compose them on
   that same URL, e.g.
   `http://localhost:8765/<name>.html#tab=reviewers&pick=a.cc,b.cc`.

A server from an earlier map serves every map in `out/codebase_map/`, so a new
map is reachable at its own filename without restarting. Re-rendering an
existing map is picked up on reload.

## Shortcuts

- **No agent prose wanted / quick look:**
  `python3 $S build <dir> --serve --no-open` does extract + render with
  placeholders and serves it in one step (run in the background; still give the
  user the URL).
- **"Who should review my CL?"** — you don't need the whole pipeline.
  `build --serve` the directory, then link
  `http://localhost:<port>/<name>.html#tab=reviewers&pick=<comma-separated paths>`.
  The planner suggests a small approver set client-side.
- **Re-annotate after a refresh:** `annotations.json` is keyed by path, so
  re-running `extract` + `annotate` with the same file keeps your prose.

## Limits

- Refuses to map the repo root; pick a directory. Very large directories
  (`third_party/blink/renderer/core`) work: the tree builds rows only as
  directories are opened, but "expand all" still builds every row.
- Only git-tracked files are mapped (untracked and generated files are left out;
  symlinks are skipped). Binary files count as 0 lines.
- Directories inside a git submodule (`v8/`, `third_party/skia/`, ...) are
  mapped with the submodule's history but Chromium's `src/` paths, OWNERS and
  output directory. Submodules *below* the mapped directory are skipped.
- OWNERS resolution follows `file://`, `file:` and `include` up to 4 levels deep
  and handles `per-file`, `set noparent`, comments and
  `#{LAST_RESORT_SUGGESTION}` (those owners are suggested only when nobody else
  can approve; the tag applies where it is written, so the same person can be a
  regular owner in a nearer OWNERS). Only the owner lines of an included file
  are used, not its `per-file` rules. An include that can't be read (missing, or
  nested deeper than 4 levels) is shown as unresolved and flagged in the
  planner, never suggested as a reviewer. Other unrecognised directives are
  ignored rather than shown as owners. `per-file` globs follow Gerrit
  code-owners' `find-owners` syntax: a rule also covers matching files in
  subdirectories. Under a `per-file ... set noparent` rule only that rule's
  owners count as approvers.
- DIR_METADATA `mixins` are applied (one level), and fields a directory doesn't
  set are inherited from the nearest parent DIR_METADATA, as `dirmd` does. The
  component shown prefers `buganizer_public`, then `buganizer`, then `monorail`.
- "Owners" shown per file are the *nearest* approvers, not every person up the
  chain who technically could approve. The full chain is in the Details tab.
- Dependency edges (`#include` / GN `deps` between subdirectories) are not
  extracted yet.
