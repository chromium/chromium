# Landmines Extension

Aims to disable commands that tend to crash or stall gemini. Hopefully we can
remove most of these in the future when agents are smart enough to not try them
in the first place.

Also contains a note about using `rg` and `fdfind` as defaults. These are
available on Debian via:

```
sudo apt-get install ripgrep fd-find
```

## Disabled Commands

The following are disabled because they are too slow on chrome's large source
tree:

- `glob`
- `search_file_content`
- `find  .`
- `ls -R`
- `grep -r`
- `grep -R`

Other disables:

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
