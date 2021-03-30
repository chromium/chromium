# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An https server that forwards requests to another server. This allows a
server that supports http only to be accessed over https.
"""

import BaseHTTPServer
import minica
import re
import socket
import SocketServer
import sys
import urllib2
import urlparse
import testserver_base
import tlslite.api


class RedirectSuppressor(urllib2.HTTPErrorProcessor):
  """Prevents urllib2 from following http redirects.

  If this class is placed in an urllib2.OpenerDirector's handler chain before
  the default urllib2.HTTPRedirectHandler, it will terminate the processing of
  responses containing redirect codes (301, 302, 303, 307) before they reach the
  default redirect handler.
  """

  def http_response(self, req, response):
    return response

  def https_response(self, req, response):
    return response


class RequestForwarder(BaseHTTPServer.BaseHTTPRequestHandler):
  """Handles requests received by forwarding them to the another server."""

  def do_GET(self):
    """Forwards GET requests."""
    self._forward(None)

  def do_POST(self):
    """Forwards POST requests."""
    self._forward(self.rfile.read(int(self.headers['Content-Length'])))

  def _forward(self, body):
    """Forwards a GET or POST request to another server.

    Args:
      body: The request body. This should be `None` for GET requests.
    """
    request_url = urlparse.urlparse(self.path)
    url = urlparse.urlunparse((self.server.forward_scheme,
                               self.server.forward_netloc,
                               self.server.forward_path + request_url[2],
                               request_url[3],
                               request_url[4],
                               request_url[5]))

    headers = dict((key, value) for key, value in dict(self.headers).iteritems()
                   if key.lower() != 'host')
    opener = urllib2.build_opener(RedirectSuppressor)
    forward = opener.open(urllib2.Request(url, body, headers))

    self.send_response(forward.getcode())
    for key, value in dict(forward.info()).iteritems():
      # RFC 6265 states in section 3:
      #
      # Origin servers SHOULD NOT fold multiple Set-Cookie header fields into
      # a single header field.
      #
      # Python 2 does not obey this requirement and folds multiple Set-Cookie
      # header fields into one. The following code undoes this folding by
      # splitting cookies into separate fields again. Note that this is a hack
      # because the code cannot reliably distinguish between commas inserted by
      # Python while folding multiple headers and commas that were part of the
      # original Set-Cookie headers. The code uses a heuristic that splits at
      # every comma followed by a space, a token and an equals sign.
      if key == 'set-cookie':
        start = 0
        # Find the next occurrence of a comma followed by a space, a token
        # (defined by RFC 2616 section 2.2 as one or more ASCII characters
        # except the characters listed in the regex below) and an equals sign.
        for match in re.finditer(r', [^\000-\037\177()<>@,:\\"/[\]?={} ]+=',
                                 value):
          end = match.start()
          if end > start:
            self.send_header(key, value[start:end])
          start = end + 2
        self.send_header(key, value[start:])
      else:
        self.send_header(key, value)
    self.end_headers()
    self.wfile.write(forward.read())


class MultiThreadedHTTPSServer(SocketServer.ThreadingMixIn,
                               tlslite.api.TLSSocketServerMixIn,
                               testserver_base.ClientRestrictingServerMixIn,
                               testserver_base.BrokenPipeHandlerMixIn,
                               testserver_base.StoppableHTTPServer):
  """A multi-threaded version of testserver.HTTPSServer."""

  def __init__(self, server_address, request_hander_class, pem_cert_and_key):
    """Initializes the server.

    Args:
      server_address: Server host and port.
      request_hander_class: The class that will handle requests to the server.
      pem_cert_and_key: Path to file containing the https cert and private key.
    """
    self.cert_chain = tlslite.api.X509CertChain()
    self.cert_chain.parsePemList(pem_cert_and_key)
    # Force using only python implementation - otherwise behavior is different
    # depending on whether m2crypto Python module is present (error is thrown
    # when it is). m2crypto uses a C (based on OpenSSL) implementation under
    # the hood.
    self.private_key = tlslite.api.parsePEMKey(pem_cert_and_key,
                                               private=True,
                                               implementations=['python'])

    testserver_base.StoppableHTTPServer.__init__(self,
                                                 server_address,
                                                 request_hander_class)

  def handshake(self, tlsConnection):
    """Performs the SSL handshake for an https connection.

    Args:
      tlsConnection: The https connection.
    Returns:
      Whether the SSL handshake succeeded.
    """
    try:
      self.tlsConnection = tlsConnection
      tlsConnection.handshakeServer(certChain=self.cert_chain,
                                    privateKey=self.private_key)
      tlsConnection.ignoreAbruptClose = True
      return True
    except:
      return False


class ServerRunner(testserver_base.TestServerRunner):
  """Runner that starts an https server which forwards requests to another
  server.
  """

  def create_server(self, server_data):
    """Performs the SSL handshake for an https connection.

    Args:
      server_data: Dictionary that holds information about the server.
    Returns:
      The started server.
    """
    # The server binds to `host:port` but the certificate is issued to
    # `ssl_host` instead.
    port = self.options.port
    host = self.options.host
    ssl_host = self.options.ssl_host

    # Allow `ssl_host` to be an IP address or a domain name, and ensure
    # it gets added as the appropriate subjectAltName of the generated
    # certificate.
    dns_sans = None
    ip_sans = None
    ip = None
    if ip is None:
      try:
        ip = socket.inet_pton(socket.AF_INET, ssl_host)
        ip_sans = [ip]
      except socket.error:
        pass
    if ip is None:
      try:
        ip = socket.inet_pton(socket.AF_INET6, ssl_host)
        ip_sans = [ip]
      except socket.error:
        pass
    if ip is None:
      dns_sans = [ssl_host]

    (pem_cert_and_key, ocsp_der) = minica.GenerateCertKeyAndOCSP(
        subject = self.options.ssl_host,
        ocsp_url = None,
        ip_sans = ip_sans,
        dns_sans = dns_sans)

    server = MultiThreadedHTTPSServer((host, port),
                                      RequestForwarder,
                                      pem_cert_and_key)
    print 'HTTPS server started on %s:%d...' % (host, server.server_port)

    forward_target = urlparse.urlparse(self.options.forward_target)
    server.forward_scheme = forward_target[0]
    server.forward_netloc = forward_target[1]
    server.forward_path = forward_target[2].rstrip('/')
    server.forward_host = forward_target.hostname
    if forward_target.port:
      server.forward_host += ':' + str(forward_target.port)
    server_data['port'] = server.server_port
    return server

  def add_options(self):
    """Specifies the command-line options understood by the server."""
    testserver_base.TestServerRunner.add_options(self)
    self.option_parser.add_option('--https', action='store_true',
                                  help='Ignored (provided for compatibility '
                                  'only).')
    self.option_parser.add_option('--ocsp', help='Ignored (provided for'
                                  'compatibility only).')
    self.option_parser.add_option('--ocsp-date', help='Ignored (provided for'
                                  'compatibility only).')
    self.option_parser.add_option('--ocsp-produced', help='Ignored (provided '
                                  'for compatibility only).')
    self.option_parser.add_option('--ssl-host', help='The host name that the '
                                  'certificate should be issued to.')
    self.option_parser.add_option('--forward-target', help='The URL prefix to '
                                  'which requests will be forwarded.')


if __name__ == '__main__':
  sys.exit(ServerRunner().main())
