Do not use the `read_many_files` tool. Read files one at a time with
`read_file`.

Any time you want to use `grep -r`, use `rg` instead.

Any time you want to use `find`, use `fdfind` instead.

If running `rg` or `fdfind` fail because the executables are missing, tell the
user to install them with the following command, and then stop.

```
sudo apt-get install ripgrep fd-find
```
