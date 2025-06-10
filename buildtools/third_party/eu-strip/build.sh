#!/bin/sh -xe

rm -rf elfutils
git clone git://sourceware.org/git/elfutils.git
cd elfutils
git checkout elfutils-0.170
autoheader
aclocal
autoconf
automake --add-missing
patch -p1 < ../fix-elf-size.patch
mkdir build
cd build
../configure --enable-maintainer-mode
make -j40
gcc -std=gnu99 -Wall -Wshadow -Wunused -Wextra -fgnu89-inline \
  -Wformat=2 -Werror -g -O2 -Wl,-rpath-link,libelf:libdw -Wl,--build-id=none -o eu-strip \
  src/strip.o libebl/libebl.a libelf/libelf.a lib/libeu.a libdw/libdw.a -ldl -lz
./eu-strip -o ../../bin/eu-strip eu-strip
