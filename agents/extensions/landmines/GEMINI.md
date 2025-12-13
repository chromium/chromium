Do not use the `read_many_files` tool. Read files one at a time with
`read_file`.

Any time you want to use `grep -r` or `git grep`, use `rg` instead.

Any time you want to use `find`, use `fdfind` instead.

If running `rg` or `fdfind` fail because the executables are missing, tell the
user to install them with the following command, and then stop.

```
sudo apt-get install ripgrep fd-find
```

Never directly install software with `brew` or `apt-get` - instead suggest the
required installation to the user with the full command line, and then stop.

Never amend git commits (with "git commit --amend"). Always create new ones.
