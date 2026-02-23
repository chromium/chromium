# Landmines Extension

Aims to disable commands that users almost never want. This extension has
become partially obsolete with policies - see `//.gemini/policies`.

Also contains a note about using `fdfind` as default. This is available on
Debian via:

```
sudo apt-get install fd-find
```

## Disabled Commands

- `git grep`
  - This runs plenty fast, but skips submodules by default. Better to just use
    other search tools.
- `git commit --amend`
  - Better to squash commits afterwards than to have gemini overwrite your
    commits.
- `gn ls`
  - Produces too much output and hangs the agent.
- `gn clean`
  - Agent sometimes tries this when builds fail. Better to debug the failed
    incremental build than do a clean build.
- `read_many_files`
  - The agent tends to echo out file contents when using this tool, which hangs
    the agent for large files (e.g. `tools/metrics/histograms/enums.xml`)
  - https://github.com/google-gemini/gemini-cli/issues/5604

## But I Want to Use a Disabled Command

You can:

1. Uninstall this extension, or
2. Write a wrapper script for the command for the agent to run.
