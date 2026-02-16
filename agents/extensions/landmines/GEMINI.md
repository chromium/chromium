Do not use the `read_many_files` tool. Read files one at a time with
`read_file`.

Any time you want to use `find`, use `fdfind` instead.

If running `fdfind` fails because the executable is missing, tell the
user to install it with the following command, and then stop.

```
sudo apt-get install fd-find
```

Never directly install software with `brew` or `apt-get` - instead suggest the
required installation to the user with the full command line, and then stop.

Never amend git commits (with "git commit --amend"). Always create new ones.
