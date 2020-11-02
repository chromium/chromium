#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A local web server with a copy of functionality of
# http://prerender-test.appspot.com with a DISABLED check whether Prerendering
# is working.

import BaseHTTPServer
import argparse
import os
import sys


THIS_DIR = os.path.abspath(os.path.dirname(__file__))


def ReadFileRelative(file_name):
  return open(os.path.join(THIS_DIR, file_name), 'r').read()


class Handler(BaseHTTPServer.BaseHTTPRequestHandler):
  def do_GET(self):
    file_path = None
    if self.path == '/':
      self.path = '/index.html'
    if self.path[0] == '/':
      self.path = self.path[1:]
    supported_file_to_mime = {
      'index.html': 'text/html',
      'default.css': 'text/css',
      'prerender.js': 'application/javascript'
    }
    if self.path in supported_file_to_mime:
      file_path = self.path

    if file_path:
      self._SendHeaders(supported_file_to_mime[file_path])
      self.wfile.write(ReadFileRelative(file_path))
      self.wfile.close()
      return

    self.send_error(404, 'Not found')

  def _SendHeaders(self, mime_type):
    self.send_response(200)
    self.send_header('Content-type', mime_type)
    self.send_header('Cache-Control', 'no-cache')
    self.end_headers()

def main(argv):
  parser = argparse.ArgumentParser(prog='prerender_test')
  parser.add_argument('-p', '--port', type=int, default=8080,
                      help='port to run on (default = %(default)s)')
  args = parser.parse_args(argv)
  server_name = 'localhost'

  s = BaseHTTPServer.HTTPServer((server_name, args.port), Handler)
  try:
    print('Listening on http://{}:{}/'.format(server_name, args.port))
    s.serve_forever()
    return 0
  except KeyboardInterrupt:
    s.server_close()
    return 130


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
