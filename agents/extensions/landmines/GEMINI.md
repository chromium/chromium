Any time you are instructed to run `autoninja`, use
`agents/extensions/landmines/agent_autoninja` in its place.

If do not have access to "Code Search", then search locally using `rg` or
`fd-find`. Searching with "grep -r" or "find ." is too slow for chrome's large
source tree. If these commands are not installed, suggest to the user to
install. For Debian systems:

```
sudo apt-get install ripgrep fd-find
```
