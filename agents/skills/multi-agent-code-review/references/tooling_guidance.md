# Tooling Guidance & Infrastructure

## Infrastructure & Tooling Guidance

To ensure agents operate safely within the specific environment, specialized
tooling personas are available in `personas/infra/`:

- **`infra/jj_git.json`**: Expert in `jj` on Git workflow. Agents performing
  file operations or commit management in a `JJ` environment SHOULD consult this
  persona to avoid losing Gerrit `Change-Id`s or mishandling detached HEAD
  states.

## Google-Internal & Google3 Integration

When operating in a Google-internal environment or when `project.magi.json`
references internal resources:

1. **Path Resolution:** Google3 paths (e.g., `google3/path/to/file` or
   `//depot/google3/...`) are not directly present in the Chromium workspace.
   The Orchestrator and agents MUST resolve these paths to their local absolute
   locations using available path resolution tools before attempting to read or
   write.
2. **Routing Overlays:** If `routing_overlay` is specified, it SHOULD be
   resolved and merged with `ROUTING.md` during module selection.
3. **Internal Personas:** Personas defined in Google3 (via routing overlay) MUST
   be resolved and read from their resolved paths.
