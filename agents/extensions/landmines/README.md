# Landmines Extension

Aims to disable commands that tend to crash or stall gemini. Hopefully we can
remove most of these in the future...

Also contains a note about using `rg` and `fdfind` as defaults. These are
available on Debian via:

```
sudo apt-get install ripgrep fd-find
```

## Disabled Commands
The following are disabled because they are too slow on chrome's large source tree:
 * `glob`
 * `search_file_content`
 * `find  .`
 * `ls -R`
 * `grep -r`
 * `grep -R`

Other disables:
* `autoninja`:
  * Replaced with `agent_autoninja.py`, which ensures `--quiet` is passed.
  * I was finding that even when told to use `--quiet`, it would often forget
    to after running for a while.
* `git grep`
  * This runs plenty fast, but skips submodules by default. Better to just use
    other search tools.
* `gn ls`
  * Produces too much output and hangs the agent.
* `read_many_files`
  * The agent tends to echo out file contents when using this tool, which hangs
    the agent for large files (e.g. `tools/metrics/histograms/enums.xml`)
