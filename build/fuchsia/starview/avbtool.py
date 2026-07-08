#!/usr/bin/env python3

# Note: This file was copied from AOSP platform/external/avb/avbtool.py.
# Source: https://android.googlesource.com/platform/external/avb/+/master/avbtool.py
# It is used for signing U-Boot environment images and generating persistent vbmeta images.

# Copyright 2016, The Android Open Source Project
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use, copy,
# modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
"""Command-line tool for working with Android Verified Boot images."""

import argparse
import binascii
import bisect
import hashlib
import json
import math
import os
import struct
import subprocess
import sys
import tempfile
import time

# Keep in sync with libavb/avb_version.h.
AVB_VERSION_MAJOR = 1
AVB_VERSION_MINOR = 3
AVB_VERSION_SUB = 0

# Keep in sync with libavb/avb_footer.h.
AVB_FOOTER_VERSION_MAJOR = 1
AVB_FOOTER_VERSION_MINOR = 0

AVB_VBMETA_IMAGE_FLAGS_HASHTREE_DISABLED = 1
AVB_VBMETA_IMAGE_FLAGS_VERIFICATION_DISABLED = 2

# Configuration for enabling logging of calls to avbtool.
AVB_INVOCATION_LOGFILE = os.environ.get('AVB_INVOCATION_LOGFILE')

# Known values for certificate "usage" field. These values must match the
# libavb_cert implementation.
#
# The "android.things" substring is only for historical reasons; these strings
# are used for the general-purpose libavb_cert extension and are not specific
# to the Android Things project. However, changing them would be a breaking
# change so it's simpler to leave them as-is.
CERT_USAGE_SIGNING = 'com.google.android.things.vboot'
CERT_USAGE_INTERMEDIATE_AUTHORITY = 'com.google.android.things.vboot.ca'
CERT_USAGE_UNLOCK = 'com.google.android.things.vboot.unlock'


class AvbError(Exception):
  """Application-specific errors.

  These errors represent issues for which a stack-trace should not be
  presented.

  Attributes:
    message: Error message.
  """

  def __init__(self, message):
    Exception.__init__(self, message)


class Algorithm(object):
  """Contains details about an algorithm.

  See the avb_vbmeta_image.h file for more details about algorithms.

  The constant |ALGORITHMS| is a dictionary from human-readable
  names (e.g 'SHA256_RSA2048') to instances of this class.

  Attributes:
    algorithm_type: Integer code corresponding to |AvbAlgorithmType|.
    hash_name: Empty or a name from |hashlib.algorithms|.
    hash_num_bytes: Number of bytes used to store the hash.
    signature_num_bytes: Number of bytes used to store the signature.
    public_key_num_bytes: Number of bytes used to store the public key.
    padding: Padding used for signature as bytes, if any.
  """

  def __init__(self, algorithm_type, hash_name, hash_num_bytes,
               signature_num_bytes, public_key_num_bytes, padding):
    self.algorithm_type = algorithm_type
    self.hash_name = hash_name
    self.hash_num_bytes = hash_num_bytes
    self.signature_num_bytes = signature_num_bytes
    self.public_key_num_bytes = public_key_num_bytes
    self.padding = padding


# This must be kept in sync with the avb_crypto.h file.
#
# The PKC1-v1.5 padding is a blob of binary DER of ASN.1 and is
# obtained from section 5.2.2 of RFC 4880.
ALGORITHMS = {
    'NONE': Algorithm(
        algorithm_type=0,        # AVB_ALGORITHM_TYPE_NONE
        hash_name='',
        hash_num_bytes=0,
        signature_num_bytes=0,
        public_key_num_bytes=0,
        padding=b''),
    'SHA256_RSA2048': Algorithm(
        algorithm_type=1,        # AVB_ALGORITHM_TYPE_SHA256_RSA2048
        hash_name='sha256',
        hash_num_bytes=32,
        signature_num_bytes=256,
        public_key_num_bytes=8 + 2*2048//8,
        padding=bytes(bytearray([
            # PKCS1-v1_5 padding
            0x00, 0x01] + [0xff]*202 + [0x00] + [
                # ASN.1 header
                0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
                0x00, 0x04, 0x20,
            ]))),
    'SHA256_RSA4096': Algorithm(
        algorithm_type=2,        # AVB_ALGORITHM_TYPE_SHA256_RSA4096
        hash_name='sha256',
        hash_num_bytes=32,
        signature_num_bytes=512,
        public_key_num_bytes=8 + 2*4096//8,
        padding=bytes(bytearray([
            # PKCS1-v1_5 padding
            0x00, 0x01] + [0xff]*458 + [0x00] + [
                # ASN.1 header
                0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
                0x00, 0x04, 0x20,
            ]))),
    'SHA256_RSA8192': Algorithm(
        algorithm_type=3,        # AVB_ALGORITHM_TYPE_SHA256_RSA8192
        hash_name='sha256',
        hash_num_bytes=32,
        signature_num_bytes=1024,
        public_key_num_bytes=8 + 2*8192//8,
        padding=bytes(bytearray([
            # PKCS1-v1_5 padding
            0x00, 0x01] + [0xff]*970 + [0x00] + [
                # ASN.1 header
                0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
                0x00, 0x04, 0x20,
            ]))),
    'SHA512_RSA2048': Algorithm(
        algorithm_type=4,        # AVB_ALGORITHM_TYPE_SHA512_RSA2048
        hash_name='sha512',
        hash_num_bytes=64,
        signature_num_bytes=256,
        public_key_num_bytes=8 + 2*2048//8,
        padding=bytes(bytearray([
            # PKCS1-v1_5 padding
            0x00, 0x01] + [0xff]*170 + [0x00] + [
                # ASN.1 header
                0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
                0x00, 0x04, 0x40
            ]))),
    'SHA512_RSA4096': Algorithm(
        algorithm_type=5,        # AVB_ALGORITHM_TYPE_SHA512_RSA4096
        hash_name='sha512',
        hash_num_bytes=64,
        signature_num_bytes=512,
        public_key_num_bytes=8 + 2*4096//8,
        padding=bytes(bytearray([
            # PKCS1-v1_5 padding
            0x00, 0x01] + [0xff]*426 + [0x00] + [
                # ASN.1 header
                0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
                0x00, 0x04, 0x40
            ]))),
    'SHA512_RSA8192': Algorithm(
        algorithm_type=6,        # AVB_ALGORITHM_TYPE_SHA512_RSA8192
        hash_name='sha512',
        hash_num_bytes=64,
        signature_num_bytes=1024,
        public_key_num_bytes=8 + 2*8192//8,
        padding=bytes(bytearray([
            # PKCS1-v1_5 padding
            0x00, 0x01] + [0xff]*938 + [0x00] + [
                # ASN.1 header
                0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
                0x00, 0x04, 0x40
            ]))),
}


def get_release_string():
  """Calculates the release string to use in the VBMeta struct."""
  # Keep in sync with libavb/avb_version.c:avb_version_string().
  return 'avbtool {}.{}.{}'.format(AVB_VERSION_MAJOR,
                                   AVB_VERSION_MINOR,
                                   AVB_VERSION_SUB)


def round_to_multiple(number, size):
  """Rounds a number up to nearest multiple of another number.

  Arguments:
    number: The number to round up.
    size: The multiple to round up to.

  Returns:
    If |number| is a multiple of |size|, returns |number|, otherwise
    returns |number| + |size|.
  """
  remainder = number % size
  if remainder == 0:
    return number
  return number + size - remainder


def round_to_pow2(number):
  """Rounds a number up to the next power of 2.

  Arguments:
    number: The number to round up.

  Returns:
    If |number| is already a power of 2 then |number| is
    returned. Otherwise the smallest power of 2 greater than |number|
    is returned.
  """
  return 2**((number - 1).bit_length())


def encode_long(num_bits, value):
  """Encodes a long to a bytearray() using a given amount of bits.

  This number is written big-endian, e.g. with the most significant
  bit first.

  This is the reverse of decode_long().

  Arguments:
    num_bits: The number of bits to write, e.g. 2048.
    value: The value to write.

  Returns:
    A bytearray() with the encoded long.
  """
  ret = bytearray()
  for bit_pos in range(num_bits, 0, -8):
    octet = (value >> (bit_pos - 8)) & 0xff
    ret.extend(struct.pack('!B', octet))
  return ret


def decode_long(blob):
  """Decodes a long from a bytearray() using a given amount of bits.

  This number is expected to be in big-endian, e.g. with the most
  significant bit first.

  This is the reverse of encode_long().

  Arguments:
    blob: A bytearray() with the encoded long.

  Returns:
    The decoded value.
  """
  ret = 0
  for b in bytearray(blob):
    ret *= 256
    ret += b
  return ret


def egcd(a, b):
  """Calculate greatest common divisor of two numbers.

  This implementation uses a recursive version of the extended
  Euclidian algorithm.

  Arguments:
    a: First number.
    b: Second number.

  Returns:
    A tuple (gcd, x, y) that where |gcd| is the greatest common
    divisor of |a| and |b| and |a|*|x| + |b|*|y| = |gcd|.
  """
  if a == 0:
    return (b, 0, 1)
  g, y, x = egcd(b % a, a)
  return (g, x - (b // a) * y, y)


def modinv(a, m):
  """Calculate modular multiplicative inverse of |a| modulo |m|.

  This calculates the number |x| such that |a| * |x| == 1 (modulo
  |m|). This number only exists if |a| and |m| are co-prime - |None|
  is returned if this isn't true.

  Arguments:
    a: The number to calculate a modular inverse of.
    m: The modulo to use.

  Returns:
    The modular multiplicative inverse of |a| and |m| or |None| if
    these numbers are not co-prime.
  """
  gcd, x, _ = egcd(a, m)
  if gcd != 1:
    return None  # modular inverse does not exist
  return x % m


def parse_number(string):
  """Parse a string as a number.

  This is just a short-hand for int(string, 0) suitable for use in the
  |type| parameter of |ArgumentParser|'s add_argument() function. An
  improvement to just using type=int is that this function supports
  numbers in other bases, e.g. "0x1234".

  Arguments:
    string: The string to parse.

  Returns:
    The parsed integer.

  Raises:
    ValueError: If the number could not be parsed.
  """
  return int(string, 0)


class RSAPublicKey(object):
  """Data structure used for a RSA public key.

  Attributes:
    exponent: The key exponent.
    modulus: The key modulus.
    num_bits: The key size.
    key_path: The path to a key file.
  """

  MODULUS_PREFIX = b'modulus='

  def __init__(self, key_path):
    """Loads and parses an RSA key from either a private or public key file.

    Arguments:
      key_path: The path to a key file.

    Raises:
      AvbError: If RSA key parameters could not be read from file.
    """
    # We used to have something as simple as this:
    #
    #  key = Crypto.PublicKey.RSA.importKey(open(key_path).read())
    #  self.exponent = key.e
    #  self.modulus = key.n
    #  self.num_bits = key.size() + 1
    #
    # but unfortunately PyCrypto is not available in the builder. So
    # instead just parse openssl(1) output to get this
    # information. It's ugly but...
    args = ['openssl', 'rsa', '-in', key_path, '-modulus', '-noout']
    p = subprocess.Popen(args,
                         stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    (pout, perr) = p.communicate()
    if p.wait() != 0:
      # Could be just a public key is passed, try that.
      args.append('-pubin')
      p = subprocess.Popen(args,
                           stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
      (pout, perr) = p.communicate()
      if p.wait() != 0:
        raise AvbError('Error getting public key: {}'.format(perr))

    if not pout.lower().startswith(self.MODULUS_PREFIX):
      raise AvbError('Unexpected modulus output')

    modulus_hexstr = pout[len(self.MODULUS_PREFIX):]

    # The exponent is assumed to always be 65537 and the number of
    # bits can be derived from the modulus by rounding up to the
    # nearest power of 2.
    self.key_path = key_path
    self.modulus = int(modulus_hexstr, 16)
    self.num_bits = round_to_pow2(int(math.ceil(math.log(self.modulus, 2))))
    self.exponent = 65537

  def encode(self):
    """Encodes the public RSA key in |AvbRSAPublicKeyHeader| format.

    This creates a |AvbRSAPublicKeyHeader| as well as the two large
    numbers (|key_num_bits| bits long) following it.

    Returns:
      The |AvbRSAPublicKeyHeader| followed by two large numbers as bytes.

    Raises:
      AvbError: If given RSA key exponent is not 65537.
    """
    if self.exponent != 65537:
      raise AvbError('Only RSA keys with exponent 65537 are supported.')
    ret = bytearray()
    # Calculate n0inv = -1/n[0] (mod 2^32)
    b = 2 ** 32
    n0inv = b - modinv(self.modulus, b)
    # Calculate rr = r^2 (mod N), where r = 2^(# of key bits)
    r = 2 ** self.modulus.bit_length()
    rrmodn = r * r % self.modulus
    ret.extend(struct.pack('!II', self.num_bits, n0inv))
    ret.extend(encode_long(self.num_bits, self.modulus))
    ret.extend(encode_long(self.num_bits, rrmodn))
    return bytes(ret)

  def sign(self, algorithm_name, data_to_sign, signing_helper=None,
           signing_helper_with_files=None):
    """Sign given data using |signing_helper| or openssl.

    openssl is used if neither the parameters signing_helper nor
    signing_helper_with_files are given.

    Arguments:
      algorithm_name: The algorithm name as per the ALGORITHMS dict.
      data_to_sign: Data to sign as bytes or bytearray.
      signing_helper: Program which signs a hash and returns the signature.
      signing_helper_with_files: Same as signing_helper but uses files instead.

    Returns:
      The signature as bytes.

    Raises:
      AvbError: If an error occurred during signing.
    """
    # Checks requested algorithm for validity.
    algorithm = ALGORITHMS.get(algorithm_name)
    if not algorithm:
      raise AvbError('Algorithm with name {} is not supported.'
                     .format(algorithm_name))

    if self.num_bits != (algorithm.signature_num_bytes * 8):
      raise AvbError('Key size of key ({} bits) does not match key size '
                     '({} bits) of given algorithm {}.'
                     .format(self.num_bits, algorithm.signature_num_bytes * 8,
                             algorithm_name))

    # Hashes the data.
    hasher = hashlib.new(algorithm.hash_name)
    hasher.update(data_to_sign)
    digest = hasher.digest()

    # Calculates the signature.
    padding_and_hash = algorithm.padding + digest
    p = None
    if signing_helper_with_files is not None:
      with tempfile.NamedTemporaryFile() as signing_file:
        signing_file.write(padding_and_hash)
        signing_file.flush()
        p = subprocess.Popen([signing_helper_with_files, algorithm_name,
                              self.key_path, signing_file.name])
        retcode = p.wait()
        if retcode != 0:
          raise AvbError('Error signing')
        signing_file.seek(0)
        signature = signing_file.read()
    else:
      if signing_helper is not None:
        p = subprocess.Popen(
            [signing_helper, algorithm_name, self.key_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
      else:
        p = subprocess.Popen(
            ['openssl', 'rsautl', '-sign', '-inkey', self.key_path, '-raw'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
      (pout, perr) = p.communicate(padding_and_hash)
      retcode = p.wait()
      if retcode != 0:
        raise AvbError('Error signing: {}'.format(perr))
      signature = pout
    if len(signature) != algorithm.signature_num_bytes:
      raise AvbError('Error signing: Invalid length of signature')
    return signature


def lookup_algorithm_by_type(alg_type):
  """Looks up algorithm by type.

  Arguments:
    alg_type: The integer representing the type.

  Returns:
    A tuple with the algorithm name and an |Algorithm| instance.

  Raises:
    Exception: If the algorithm cannot be found
  """
  for alg_name in ALGORITHMS:
    alg_data = ALGORITHMS[alg_name]
    if alg_data.algorithm_type == alg_type:
      return (alg_name, alg_data)
  raise AvbError('Unknown algorithm type {}'.format(alg_type))


def lookup_hash_size_by_type(alg_type):
  """Looks up hash size by type.

  Arguments:
    alg_type: The integer representing the type.

  Returns:
    The corresponding hash size.

  Raises:
    AvbError: If the algorithm cannot be found.
  """
  for alg_name in ALGORITHMS:
    alg_data = ALGORITHMS[alg_name]
    if alg_data.algorithm_type == alg_type:
      return alg_data.hash_num_bytes
  raise AvbError('Unsupported algorithm type {}'.format(alg_type))


def verify_vbmeta_signature(vbmeta_header, vbmeta_blob):
  """Checks that signature in a vbmeta blob was made by the embedded public key.

  Arguments:
    vbmeta_header: A AvbVBMetaHeader.
    vbmeta_blob: The whole vbmeta blob, including the header as bytes or
        bytearray.

  Returns:
    True if the signature is valid and corresponds to the embedded
    public key. Also returns True if the vbmeta blob is not signed.

  Raises:
    AvbError: If there errors calling out to openssl command during
        signature verification.
  """
  (_, alg) = lookup_algorithm_by_type(vbmeta_header.algorithm_type)
  if not alg.hash_name:
    return True
  header_blob = vbmeta_blob[0:256]
  auth_offset = 256
  aux_offset = auth_offset + vbmeta_header.authentication_data_block_size
  aux_size = vbmeta_header.auxiliary_data_block_size
  aux_blob = vbmeta_blob[aux_offset:aux_offset + aux_size]
  pubkey_offset = aux_offset + vbmeta_header.public_key_offset
  pubkey_size = vbmeta_header.public_key_size
  pubkey_blob = vbmeta_blob[pubkey_offset:pubkey_offset + pubkey_size]

  digest_offset = auth_offset + vbmeta_header.hash_offset
  digest_size = vbmeta_header.hash_size
  digest_blob = vbmeta_blob[digest_offset:digest_offset + digest_size]

  sig_offset = auth_offset + vbmeta_header.signature_offset
  sig_size = vbmeta_header.signature_size
  sig_blob = vbmeta_blob[sig_offset:sig_offset + sig_size]

  # Now that we've got the stored digest, public key, and signature
  # all we need to do is to verify. This is the exactly the same
  # steps as performed in the avb_vbmeta_image_verify() function in
  # libavb/avb_vbmeta_image.c.

  ha = hashlib.new(alg.hash_name)
  ha.update(header_blob)
  ha.update(aux_blob)
  computed_digest = ha.digest()

  if computed_digest != digest_blob:
    return False

  padding_and_digest = alg.padding + computed_digest

  (num_bits,) = struct.unpack('!I', pubkey_blob[0:4])
  modulus_blob = pubkey_blob[8:8 + num_bits//8]
  modulus = decode_long(modulus_blob)
  exponent = 65537

  # We used to have this:
  #
  #  import Crypto.PublicKey.RSA
  #  key = Crypto.PublicKey.RSA.construct((modulus, long(exponent)))
  #  if not key.verify(decode_long(padding_and_digest),
  #                    (decode_long(sig_blob), None)):
  #    return False
  #  return True
  #
  # but since 'avbtool verify_image' is used on the builders we don't want
  # to rely on Crypto.PublicKey.RSA. Instead just use openssl(1) to verify.
  asn1_str = ('asn1=SEQUENCE:pubkeyinfo\n'
              '\n'
              '[pubkeyinfo]\n'
              'algorithm=SEQUENCE:rsa_alg\n'
              'pubkey=BITWRAP,SEQUENCE:rsapubkey\n'
              '\n'
              '[rsa_alg]\n'
              'algorithm=OID:rsaEncryption\n'
              'parameter=NULL\n'
              '\n'
              '[rsapubkey]\n'
              'n=INTEGER:{}\n'
              'e=INTEGER:{}\n').format(hex(modulus).rstrip('L'),
                                       hex(exponent).rstrip('L'))

  with tempfile.NamedTemporaryFile() as asn1_tmpfile:
    asn1_tmpfile.write(asn1_str.encode('ascii'))
    asn1_tmpfile.flush()

    with tempfile.NamedTemporaryFile() as der_tmpfile:
      p = subprocess.Popen(
          ['openssl', 'asn1parse', '-genconf', asn1_tmpfile.name, '-out',
           der_tmpfile.name, '-noout'])
      retcode = p.wait()
      if retcode != 0:
        raise AvbError('Error generating DER file')

      p = subprocess.Popen(
          ['openssl', 'rsautl', '-verify', '-pubin', '-inkey', der_tmpfile.name,
           '-keyform', 'DER', '-raw'],
          stdin=subprocess.PIPE,
          stdout=subprocess.PIPE,
          stderr=subprocess.PIPE)
      (pout, perr) = p.communicate(sig_blob)
      retcode = p.wait()
      if retcode != 0:
        raise AvbError('Error verifying data: {}'.format(perr))
      if pout != padding_and_digest:
        sys.stderr.write('Signature not correct\n')
        return False
  return True


def create_avb_hashtree_hasher(algorithm, salt):
  """Create the hasher for AVB hashtree based on the input algorithm."""

  if algorithm.lower() == 'blake2b-256':
    return hashlib.new('blake2b', salt, digest_size=32)

  return hashlib.new(algorithm, salt)


class ImageChunk(object):
  """Data structure used for representing chunks in Android sparse files.

  Attributes:
    chunk_type: One of TYPE_RAW, TYPE_FILL, or TYPE_DONT_CARE.
    chunk_offset: Offset in the sparse file where this chunk begins.
    output_offset: Offset in de-sparsified file where output begins.
    output_size: Number of bytes in output.
    input_offset: Offset in sparse file for data if TYPE_RAW otherwise None.
    fill_data: Blob with data to fill if TYPE_FILL otherwise None.
  """

  FORMAT = '<2H2I'
  TYPE_RAW = 0xcac1
  TYPE_FILL = 0xcac2
  TYPE_DONT_CARE = 0xcac3
  TYPE_CRC32 = 0xcac4

  def __init__(self, chunk_type, chunk_offset, output_offset, output_size,
               input_offset, fill_data):
    """Initializes an ImageChunk object.

    Arguments:
      chunk_type: One of TYPE_RAW, TYPE_FILL, or TYPE_DONT_CARE.
      chunk_offset: Offset in the sparse file where this chunk begins.
      output_offset: Offset in de-sparsified file.
      output_size: Number of bytes in output.
      input_offset: Offset in sparse file if TYPE_RAW otherwise None.
      fill_data: Blob as bytes with data to fill if TYPE_FILL otherwise None.

    Raises:
      ValueError: If given chunk parameters are invalid.
    """
    self.chunk_type = chunk_type
    self.chunk_offset = chunk_offset
    self.output_offset = output_offset
    self.output_size = output_size
    self.input_offset = input_offset
    self.fill_data = fill_data
    # Check invariants.
    if self.chunk_type == self.TYPE_RAW:
      if self.fill_data is not None:
        raise ValueError('RAW chunk cannot have fill_data set.')
      if not self.input_offset:
        raise ValueError('RAW chunk must have input_offset set.')
    elif self.chunk_type == self.TYPE_FILL:
      if self.fill_data is None:
        raise ValueError('FILL chunk must have fill_data set.')
      if self.input_offset:
        raise ValueError('FILL chunk cannot have input_offset set.')
    elif self.chunk_type == self.TYPE_DONT_CARE:
      if self.fill_data is not None:
        raise ValueError('DONT_CARE chunk cannot have fill_data set.')
      if self.input_offset:
        raise ValueError('DONT_CARE chunk cannot have input_offset set.')
    else:
      raise ValueError('Invalid chunk type')


class ImageHandler(object):
  """Abstraction for image I/O with support for Android sparse images.

  This class provides an interface for working with image files that
  may be using the Android Sparse Image format. When an instance is
  constructed, we test whether it's an Android sparse file. If so,
  operations will be on the sparse file by interpreting the sparse
  format, otherwise they will be directly on the file. Either way the
  operations do the same.

  For reading, this interface mimics a file object - it has seek(),
  tell(), and read() methods. For writing, only truncation
  (truncate()) and appending is supported (append_raw() and
  append_dont_care()). Additionally, data can only be written in units
  of the block size.

  Attributes:
    filename: Name of file.
    is_sparse: Whether the file being operated on is sparse.
    block_size: The block size, typically 4096.
    image_size: The size of the unsparsified file.
  """
  # See system/core/libsparse/sparse_format.h for details.
  MAGIC = 0xed26ff3a
  HEADER_FORMAT = '<I4H4I'

  # These are formats and offset of just the |total_chunks| and
  # |total_blocks| fields.
  NUM_CHUNKS_AND_BLOCKS_FORMAT = '<II'
  NUM_CHUNKS_AND_BLOCKS_OFFSET = 16

  def __init__(self, image_filename, read_only=False):
    """Initializes an image handler.

    Arguments:
      image_filename: The name of the file to operate on.
      read_only: True if file is only opened for read-only operations.

    Raises:
      ValueError: If data in the file is invalid.
    """
    self.filename = image_filename
    self._num_total_blocks = 0
    self._num_total_chunks = 0
    self._file_pos = 0
    self._read_only = read_only
    self._read_header()

  def _read_header(self):
    """Initializes internal data structures used for reading file.

    This may be called multiple times and is typically called after
    modifying the file (e.g. appending, truncation).

    Raises:
      ValueError: If data in the file is invalid.
    """
    self.is_sparse = False
    self.block_size = 4096
    self._file_pos = 0
    if self._read_only:
      self._image = open(self.filename, 'rb')
    else:
      self._image = open(self.filename, 'r+b')
    self._image.seek(0, os.SEEK_END)
    self.image_size = self._image.tell()

    self._image.seek(0, os.SEEK_SET)
    header_bin = self._image.read(struct.calcsize(self.HEADER_FORMAT))
    (magic, major_version, minor_version, file_hdr_sz, chunk_hdr_sz,
     block_size, self._num_total_blocks, self._num_total_chunks,
     _) = struct.unpack(self.HEADER_FORMAT, header_bin)
    if magic != self.MAGIC:
      # Not a sparse image, our job here is done.
      return
    if not (major_version == 1 and minor_version == 0):
      raise ValueError('Encountered sparse image format version {}.{} but '
                       'only 1.0 is supported'.format(major_version,
                                                      minor_version))
    if file_hdr_sz != struct.calcsize(self.HEADER_FORMAT):
      raise ValueError('Unexpected file_hdr_sz value {}.'.
                       format(file_hdr_sz))
    if chunk_hdr_sz != struct.calcsize(ImageChunk.FORMAT):
      raise ValueError('Unexpected chunk_hdr_sz value {}.'.
                       format(chunk_hdr_sz))

    self.block_size = block_size

    # Build an list of chunks by parsing the file.
    self._chunks = []

    # Find the smallest offset where only "Don't care" chunks
    # follow. This will be the size of the content in the sparse
    # image.
    offset = 0
    output_offset = 0
    for _ in range(1, self._num_total_chunks + 1):
      chunk_offset = self._image.tell()

      header_bin = self._image.read(struct.calcsize(ImageChunk.FORMAT))
      (chunk_type, _, chunk_sz, total_sz) = struct.unpack(ImageChunk.FORMAT,
                                                          header_bin)
      data_sz = total_sz - struct.calcsize(ImageChunk.FORMAT)

      if chunk_type == ImageChunk.TYPE_RAW:
        if data_sz != (chunk_sz * self.block_size):
          raise ValueError('Raw chunk input size ({}) does not match output '
                           'size ({})'.
                           format(data_sz, chunk_sz*self.block_size))
        self._chunks.append(ImageChunk(ImageChunk.TYPE_RAW,
                                       chunk_offset,
                                       output_offset,
                                       chunk_sz*self.block_size,
                                       self._image.tell(),
                                       None))
        self._image.seek(data_sz, os.SEEK_CUR)

      elif chunk_type == ImageChunk.TYPE_FILL:
        if data_sz != 4:
          raise ValueError('Fill chunk should have 4 bytes of fill, but this '
                           'has {}'.format(data_sz))
        fill_data = self._image.read(4)
        self._chunks.append(ImageChunk(ImageChunk.TYPE_FILL,
                                       chunk_offset,
                                       output_offset,
                                       chunk_sz*self.block_size,
                                       None,
                                       fill_data))
      elif chunk_type == ImageChunk.TYPE_DONT_CARE:
        if data_sz != 0:
          raise ValueError('Don\'t care chunk input size is non-zero ({})'.
                           format(data_sz))
        self._chunks.append(ImageChunk(ImageChunk.TYPE_DONT_CARE,
                                       chunk_offset,
                                       output_offset,
                                       chunk_sz*self.block_size,
                                       None,
                                       None))
      elif chunk_type == ImageChunk.TYPE_CRC32:
        if data_sz != 4:
          raise ValueError('CRC32 chunk should have 4 bytes of CRC, but '
                           'this has {}'.format(data_sz))
        self._image.read(4)
      else:
        raise ValueError('Unknown chunk type {}'.format(chunk_type))

      offset += chunk_sz
      output_offset += chunk_sz*self.block_size

    # Record where sparse data end.
    self._sparse_end = self._image.tell()

    # Now that we've traversed all chunks, sanity check.
    if self._num_total_blocks != offset:
      raise ValueError('The header said we should have {} output blocks, '
                       'but we saw {}'.format(self._num_total_blocks, offset))
    junk_len = len(self._image.read())
    if junk_len > 0:
      raise ValueError('There were {} bytes of extra data at the end of the '
                       'file.'.format(junk_len))

    # Assign |image_size|.
    self.image_size = output_offset

    # This is used when bisecting in read() to find the initial slice.
    self._chunk_output_offsets = [i.output_offset for i in self._chunks]

    self.is_sparse = True

  def _update_chunks_and_blocks(self):
    """Helper function to update the image header.

    The the |total_chunks| and |total_blocks| fields in the header
    will be set to value of the |_num_total_blocks| and
    |_num_total_chunks| attributes.

    """
    self._image.seek(self.NUM_CHUNKS_AND_BLOCKS_OFFSET, os.SEEK_SET)
    self._image.write(struct.pack(self.NUM_CHUNKS_AND_BLOCKS_FORMAT,
                                  self._num_total_blocks,
                                  self._num_total_chunks))

  def append_dont_care(self, num_bytes):
    """Appends a DONT_CARE chunk to the sparse file.

    The given number of bytes must be a multiple of the block size.

    Arguments:
      num_bytes: Size in number of bytes of the DONT_CARE chunk.

    Raises:
      OSError: If ImageHandler was initialized in read-only mode.
    """
    assert num_bytes % self.block_size == 0

    if self._read_only:
      raise OSError('ImageHandler is in read-only mode.')

    if not self.is_sparse:
      self._image.seek(0, os.SEEK_END)
      # This is more efficient that writing NUL bytes since it'll add
      # a hole on file systems that support sparse files (native
      # sparse, not Android sparse).
      self._image.truncate(self._image.tell() + num_bytes)
      self._read_header()
      return

    self._num_total_chunks += 1
    self._num_total_blocks += num_bytes // self.block_size
    self._update_chunks_and_blocks()

    self._image.seek(self._sparse_end, os.SEEK_SET)
    self._image.write(struct.pack(ImageChunk.FORMAT,
                                  ImageChunk.TYPE_DONT_CARE,
                                  0,  # Reserved
                                  num_bytes // self.block_size,
                                  struct.calcsize(ImageChunk.FORMAT)))
    self._read_header()

  def append_raw(self, data, multiple_block_size=True):
    """Appends a RAW chunk to the sparse file.

    The length of the given data must be a multiple of the block size,
    unless |multiple_block_size| is False.

    Arguments:
      data: Data to append as bytes.
      multiple_block_size: whether to check the length of the
        data is a multiple of the block size.

    Raises:
      OSError: If ImageHandler was initialized in read-only mode.
    """
    if multiple_block_size:
      assert len(data) % self.block_size == 0

    if self._read_only:
      raise OSError('ImageHandler is in read-only mode.')

    if not self.is_sparse:
      self._image.seek(0, os.SEEK_END)
      self._image.write(data)
      self._read_header()
      return

    self._num_total_chunks += 1
    self._num_total_blocks += len(data) // self.block_size
    self._update_chunks_and_blocks()

    self._image.seek(self._sparse_end, os.SEEK_SET)
    self._image.write(struct.pack(ImageChunk.FORMAT,
                                  ImageChunk.TYPE_RAW,
                                  0,  # Reserved
                                  len(data) // self.block_size,
                                  len(data) +
                                  struct.calcsize(ImageChunk.FORMAT)))
    self._image.write(data)
    self._read_header()

  def append_fill(self, fill_data, size):
    """Appends a fill chunk to the sparse file.

    The total length of the fill data must be a multiple of the block size.

    Arguments:
      fill_data: Fill data to append - must be four bytes.
      size: Number of chunk - must be a multiple of four and the block size.

    Raises:
      OSError: If ImageHandler was initialized in read-only mode.
    """
    assert len(fill_data) == 4
    assert size % 4 == 0
    assert size % self.block_size == 0

    if self._read_only:
      raise OSError('ImageHandler is in read-only mode.')

    if not self.is_sparse:
      self._image.seek(0, os.SEEK_END)
      self._image.write(fill_data * (size//4))
      self._read_header()
      return

    self._num_total_chunks += 1
    self._num_total_blocks += size // self.block_size
    self._update_chunks_and_blocks()

    self._image.seek(self._sparse_end, os.SEEK_SET)
    self._image.write(struct.pack(ImageChunk.FORMAT,
                                  ImageChunk.TYPE_FILL,
                                  0,  # Reserved
                                  size // self.block_size,
                                  4 + struct.calcsize(ImageChunk.FORMAT)))
    self._image.write(fill_data)
    self._read_header()

  def seek(self, offset):
    """Sets the cursor position for reading from unsparsified file.

    Arguments:
      offset: Offset to seek to from the beginning of the file.

    Raises:
      RuntimeError: If the given offset is negative.
    """
    if offset < 0:
      raise RuntimeError('Seeking with negative offset: {}'.format(offset))
    self._file_pos = offset

  def read(self, size):
    """Reads data from the unsparsified file.

    This method may return fewer than |size| bytes of data if the end
    of the file was encountered.

    The file cursor for reading is advanced by the number of bytes
    read.

    Arguments:
      size: Number of bytes to read.

    Returns:
      The data as bytes.
    """
    if not self.is_sparse:
      self._image.seek(self._file_pos)
      data = self._image.read(size)
      self._file_pos += len(data)
      return data

    # Iterate over all chunks.
    chunk_idx = bisect.bisect_right(self._chunk_output_offsets,
                                    self._file_pos) - 1
    data = bytearray()
    to_go = size
    while to_go > 0:
      chunk = self._chunks[chunk_idx]
      chunk_pos_offset = self._file_pos - chunk.output_offset
      chunk_pos_to_go = min(chunk.output_size - chunk_pos_offset, to_go)

      if chunk.chunk_type == ImageChunk.TYPE_RAW:
        self._image.seek(chunk.input_offset + chunk_pos_offset)
        data.extend(self._image.read(chunk_pos_to_go))
      elif chunk.chunk_type == ImageChunk.TYPE_FILL:
        all_data = chunk.fill_data*(chunk_pos_to_go // len(chunk.fill_data) + 2)
        offset_mod = chunk_pos_offset % len(chunk.fill_data)
        data.extend(all_data[offset_mod:(offset_mod + chunk_pos_to_go)])
      else:
        assert chunk.chunk_type == ImageChunk.TYPE_DONT_CARE
        data.extend(b'\0' * chunk_pos_to_go)

      to_go -= chunk_pos_to_go
      self._file_pos += chunk_pos_to_go
      chunk_idx += 1
      # Generate partial read in case of EOF.
      if chunk_idx >= len(self._chunks):
        break

    return bytes(data)

  def tell(self):
    """Returns the file cursor position for reading from unsparsified file.

    Returns:
      The file cursor position for reading.
    """
    return self._file_pos

  def truncate(self, size):
    """Truncates the unsparsified file.

    Arguments:
      size: Desired size of unsparsified file.

    Raises:
      ValueError: If desired size isn't a multiple of the block size.
      OSError: If ImageHandler was initialized in read-only mode.
    """
    if self._read_only:
      raise OSError('ImageHandler is in read-only mode.')

    if not self.is_sparse:
      self._image.truncate(size)
      self._read_header()
      return

    if size % self.block_size != 0:
      raise ValueError('Cannot truncate to a size which is not a multiple '
                       'of the block size')

    if size == self.image_size:
      # Trivial where there's nothing to do.
      return

    if size < self.image_size:
      chunk_idx = bisect.bisect_right(self._chunk_output_offsets, size) - 1
      chunk = self._chunks[chunk_idx]
      if chunk.output_offset != size:
        # Truncation in the middle of a trunk - need to keep the chunk
        # and modify it.
        chunk_idx_for_update = chunk_idx + 1
        num_to_keep = size - chunk.output_offset
        assert num_to_keep % self.block_size == 0
        if chunk.chunk_type == ImageChunk.TYPE_RAW:
          truncate_at = (chunk.chunk_offset +
                         struct.calcsize(ImageChunk.FORMAT) + num_to_keep)
          data_sz = num_to_keep
        elif chunk.chunk_type == ImageChunk.TYPE_FILL:
          truncate_at = (chunk.chunk_offset +
                         struct.calcsize(ImageChunk.FORMAT) + 4)
          data_sz = 4
        else:
          assert chunk.chunk_type == ImageChunk.TYPE_DONT_CARE
          truncate_at = chunk.chunk_offset + struct.calcsize(ImageChunk.FORMAT)
          data_sz = 0
        chunk_sz = num_to_keep // self.block_size
        total_sz = data_sz + struct.calcsize(ImageChunk.FORMAT)
        self._image.seek(chunk.chunk_offset)
        self._image.write(struct.pack(ImageChunk.FORMAT,
                                      chunk.chunk_type,
                                      0,  # Reserved
                                      chunk_sz,
                                      total_sz))
        chunk.output_size = num_to_keep
      else:
        # Truncation at trunk boundary.
        truncate_at = chunk.chunk_offset
        chunk_idx_for_update = chunk_idx

      self._num_total_chunks = chunk_idx_for_update
      self._num_total_blocks = 0
      for i in range(0, chunk_idx_for_update):
        self._num_total_blocks += self._chunks[i].output_size // self.block_size
      self._update_chunks_and_blocks()
      self._image.truncate(truncate_at)

      # We've modified the file so re-read all data.
      self._read_header()
    else:
      # Truncating to grow - just add a DONT_CARE section.
      self.append_dont_care(size - self.image_size)


class AvbDescriptor(object):
  """Class for AVB descriptor.

  See the |AvbDescriptor| C struct for more information.

  Attributes:
    tag: The tag identifying what kind of descriptor this is.
    data: The data in the descriptor.
  """

  SIZE = 16
  FORMAT_STRING = ('!QQ')  # tag, num_bytes_following (descriptor header)

  def __init__(self, data):
    """Initializes a new property descriptor.

    Arguments:
      data: If not None, must be a bytearray().

    Raises:
      LookupError: If the given descriptor is malformed.
    """
    assert struct.calcsize(self.FORMAT_STRING) == self.SIZE

    if data:
      (self.tag, num_bytes_following) = (
          struct.unpack(self.FORMAT_STRING, data[0:self.SIZE]))
      self.data = data[self.SIZE:self.SIZE + num_bytes_following]
    else:
      self.tag = None
      self.data = None

  def print_desc(self, o):
    """Print the descriptor.

    Arguments:
      o: The object to write the output to.
    """
    o.write('    Unknown descriptor:\n')
    o.write('      Tag:  {}\n'.format(self.tag))
    if len(self.data) < 256:
      o.write('      Data: {} ({} bytes)\n'.format(
          repr(str(self.data)), len(self.data)))
    else:
      o.write('      Data: {} bytes\n'.format(len(self.data)))

  def encode(self):
    """Serializes the descriptor.

    Returns:
      A bytearray() with the descriptor data.
    """
    num_bytes_following = len(self.data)
    nbf_with_padding = round_to_multiple(num_bytes_following, 8)
    padding_size = nbf_with_padding - num_bytes_following
    desc = struct.pack(self.FORMAT_STRING, self.tag, nbf_with_padding)
    padding = struct.pack(str(padding_size) + 'x')
    ret = desc + self.data + padding
    return bytearray(ret)

  def verify(self, image_dir, image_ext, expected_chain_partitions_map,
             image_containing_descriptor, accept_zeroed_hashtree):
    """Verifies contents of the descriptor - used in verify_image sub-command.

    Arguments:
      image_dir: The directory of the file being verified.
      image_ext: The extension of the file being verified (e.g. '.img').
      expected_chain_partitions_map: A map from partition name to the
          tuple (rollback_index_location, key_blob).
      image_containing_descriptor: The image the descriptor is in.
      accept_zeroed_hashtree: If True, don't fail if hashtree or FEC data is
          zeroed out.

    Returns:
      True if the descriptor verifies, False otherwise.
    """
    # Deletes unused parameters to prevent pylint warning unused-argument.
    del image_dir, image_ext, expected_chain_partitions_map
    del image_containing_descriptor, accept_zeroed_hashtree

    # Nothing to do.
    return True


class AvbPropertyDescriptor(AvbDescriptor):
  """A class for property descriptors.

  See the |AvbPropertyDescriptor| C struct for more information.

  Attributes:
    key: The key as string.
    value: The value as bytes.
  """

  TAG = 0
  SIZE = 32
  FORMAT_STRING = ('!QQ'  # tag, num_bytes_following (descriptor header)
                   'Q'    # key size (bytes)
                   'Q')   # value size (bytes)

  def __init__(self, data=None):
    """Initializes a new property descriptor.

    Arguments:
      data: If not None, must be as bytes of size |SIZE|.

    Raises:
      LookupError: If the given descriptor is malformed.
    """
    super().__init__(None)
    assert struct.calcsize(self.FORMAT_STRING) == self.SIZE

    if data:
      (tag, num_bytes_following, key_size,
       value_size) = struct.unpack(self.FORMAT_STRING, data[0:self.SIZE])
      expected_size = round_to_multiple(
          self.SIZE - 16 + key_size + 1 + value_size + 1, 8)
      if tag != self.TAG or num_bytes_following != expected_size:
        raise LookupError('Given data does not look like a property '
                          'descriptor.')
      try:
        self.key = data[self.SIZE:(self.SIZE + key_size)].decode('utf-8')
      except UnicodeDecodeError as e:
        raise LookupError('Key cannot be decoded as UTF-8: {}.'
                          .format(e)) from e
      self.value = data[(self.SIZE + key_size + 1):(self.SIZE + key_size + 1 +
                                                    value_size)]
    else:
      self.key = ''
      self.value = b''

  def print_desc(self, o):
    """Print the descriptor.

    Arguments:
      o: The object to write the output to.
    """
    # Go forward with python 3, bytes are represented with the 'b' prefix,
    # e.g. b'foobar'. Thus, we trim off the 'b' to keep the print output
    # the same between python 2 and python 3.
    printable_value = repr(self.value)
    if printable_value.startswith('b\''):
      printable_value = printable_value[1:]

    if len(self.value) < 256:
      o.write('    Prop: {} -> {}\n'.format(self.key, printable_value))
    else:
      o.write('    Prop: {} -> ({} bytes)\n'.format(self.key, len(self.value)))

  def encode(self):
    """Serializes the descriptor.

    Returns:
      The descriptor data as bytes.
    """
    key_encoded = self.key.encode('utf-8')
    num_bytes_following = (
        self.SIZE + len(key_encoded) + len(self.value) + 2 - 16)
    nbf_with_padding = round_to_multiple(num_bytes_following, 8)
    padding_size = nbf_with_padding - num_bytes_following
    desc = struct.pack(self.FORMAT_STRING, self.TAG, nbf_with_padding,
                       len(key_encoded), len(self.value))
    ret = (desc + key_encoded + b'\0' + self.value + b'\0' +
           padding_size * b'\0')
    return ret

  def verify(self, image_dir, image_ext, expected_chain_partitions_map,
             image_containing_descriptor, accept_zeroed_hashtree):
    """Verifies contents of the descriptor - used in verify_image sub-command.

    Arguments:
      image_dir: The directory of the file being verified.
      image_ext: The extension of the file being verified (e.g. '.img').
      expected_chain_partitions_map: A map from partition name to the
          tuple (rollback_index_location, key_blob).
      image_containing_descriptor: The image the descriptor is in.
      accept_zeroed_hashtree: If True, don't fail if hashtree or FEC data is
          zeroed out.

    Returns:
      True if the descriptor verifies, False otherwise.
    """
    # Nothing to do.
    return True


class AvbHashtreeDescriptor(AvbDescriptor):
  """A class for hashtree descriptors.

  See the |AvbHashtreeDescriptor| C struct for more information.

  Attributes:
    dm_verity_version: dm-verity version used.
    image_size: Size of the image, after rounding up to |block_size|.
    tree_offset: Offset of the hash tree in the file.
    tree_size: Size of the tree.
    data_block_size: Data block size.
    hash_block_size: Hash block size.
    fec_num_roots: Number of roots used for FEC (0 if FEC is not used).
    fec_offset: Offset of FEC data (0 if FEC is not used).
    fec_size: Size of FEC data (0 if FEC is not used).
    hash_algorithm: Hash algorithm used as string.
    partition_name: Partition name as string.
    salt: Salt used as bytes.
    root_digest: Root digest as bytes.
    flags: Descriptor flags (see avb_hashtree_descriptor.h).
  """

  TAG = 1
  RESERVED = 60
  SIZE = 120 + RESERVED
  FORMAT_STRING = ('!QQ'  # tag, num_bytes_following (descriptor header)
                   'L'    # dm-verity version used
                   'Q'    # image size (bytes)
                   'Q'    # tree offset (bytes)
                   'Q'    # tree size (bytes)
                   'L'    # data block size (bytes)
                   'L'    # hash block size (bytes)
                   'L'    # FEC number of roots
                   'Q'    # FEC offset (bytes)
                   'Q'    # FEC size (bytes)
                   '32s'  # hash algorithm used
                   'L'    # partition name (bytes)
                   'L'    # salt length (bytes)
                   'L'    # root digest length (bytes)
                   'L' +  # flags
                   str(RESERVED) + 's')  # reserved

  FLAGS_DO_NOT_USE_AB = (1 << 0)
  FLAGS_CHECK_AT_MOST_ONCE = (1 << 1)

  def __init__(self, data=None):
    """Initializes a new hashtree descriptor.

    Arguments:
      data: If not None, must be bytes of size |SIZE|.

    Raises:
      LookupError: If the given descriptor is malformed.
    """
    super().__init__(None)
    assert struct.calcsize(self.FORMAT_STRING) == self.SIZE

    if data:
      (tag, num_bytes_following, self.dm_verity_version, self.image_size,
       self.tree_offset, self.tree_size, self.data_block_size,
       self.hash_block_size, self.fec_num_roots, self.fec_offset, self.fec_size,
       self.hash_algorithm, partition_name_len, salt_len,
       root_digest_len, self.flags, _) = struct.unpack(self.FORMAT_STRING,
                                                       data[0:self.SIZE])
      expected_size = round_to_multiple(
          self.SIZE - 16 + partition_name_len + salt_len + root_digest_len, 8)
      if tag != self.TAG or num_bytes_following != expected_size:
        raise LookupError('Given data does not look like a hashtree '
                          'descriptor.')
      # Nuke NUL-bytes at the end.
      self.hash_algorithm = self.hash_algorithm.rstrip(b'\0').decode('ascii')
      o = 0
      try:
        self.partition_name = data[
            (self.SIZE + o):(self.SIZE + o + partition_name_len)
        ].decode('utf-8')
      except UnicodeDecodeError as e:
        raise LookupError('Partition name cannot be decoded as UTF-8: {}.'
                          .format(e)) from e
      o += partition_name_len
      self.salt = data[(self.SIZE + o):(self.SIZE + o + salt_len)]
      o += salt_len
      self.root_digest = data[(self.SIZE + o):(self.SIZE + o + root_digest_len)]

      if root_digest_len != self._hashtree_digest_size():
        if root_digest_len != 0:
          raise LookupError('root_digest_len doesn\'t match hash algorithm')

    else:
      self.dm_verity_version = 0
      self.image_size = 0
      self.tree_offset = 0
      self.tree_size = 0
      self.data_block_size = 0
      self.hash_block_size = 0
      self.fec_num_roots = 0
      self.fec_offset = 0
      self.fec_size = 0
      self.hash_algorithm = ''
      self.partition_name = ''
      self.salt = b''
      self.root_digest = b''
      self.flags = 0

  def _hashtree_digest_size(self):
    return len(create_avb_hashtree_hasher(self.hash_algorithm, b'').digest())

  def print_desc(self, o):
    """Print the descriptor.

    Arguments:
      o: The object to write the output to.
    """
    o.write('    Hashtree descriptor:\n')
    o.write('      Version of dm-verity:  {}\n'.format(self.dm_verity_version))
    o.write('      Image Size:            {} bytes\n'.format(self.image_size))
    o.write('      Tree Offset:           {}\n'.format(self.tree_offset))
    o.write('      Tree Size:             {} bytes\n'.format(self.tree_size))
    o.write('      Data Block Size:       {} bytes\n'.format(
        self.data_block_size))
    o.write('      Hash Block Size:       {} bytes\n'.format(
        self.hash_block_size))
    o.write('      FEC num roots:         {}\n'.format(self.fec_num_roots))
    o.write('      FEC offset:            {}\n'.format(self.fec_offset))
    o.write('      FEC size:              {} bytes\n'.format(self.fec_size))
    o.write('      Hash Algorithm:        {}\n'.format(self.hash_algorithm))
    o.write('      Partition Name:        {}\n'.format(self.partition_name))
    o.write('      Salt:                  {}\n'.format(self.salt.hex()))
    o.write('      Root Digest:           {}\n'.format(self.root_digest.hex()))
    o.write('      Flags:                 {}\n'.format(self.flags))

  def encode(self):
    """Serializes the descriptor.

    Returns:
      The descriptor data as bytes.
    """
    hash_algorithm_encoded = self.hash_algorithm.encode('ascii')
    partition_name_encoded = self.partition_name.encode('utf-8')
    num_bytes_following = (self.SIZE + len(partition_name_encoded)
                           + len(self.salt) + len(self.root_digest) - 16)
    nbf_with_padding = round_to_multiple(num_bytes_following, 8)
    padding_size = nbf_with_padding - num_bytes_following
    desc = struct.pack(self.FORMAT_STRING, self.TAG, nbf_with_padding,
                       self.dm_verity_version, self.image_size,
                       self.tree_offset, self.tree_size, self.data_block_size,
                       self.hash_block_size, self.fec_num_roots,
                       self.fec_offset, self.fec_size, hash_algorithm_encoded,
                       len(partition_name_encoded), len(self.salt),
                       len(self.root_digest), self.flags, self.RESERVED * b'\0')
    ret = (desc + partition_name_encoded + self.salt + self.root_digest +
           padding_size * b'\0')
    return ret

  def verify(self, image_dir, image_ext, expected_chain_partitions_map,
             image_containing_descriptor, accept_zeroed_hashtree):
    """Verifies contents of the descriptor - used in verify_image sub-command.

    Arguments:
      image_dir: The directory of the file being verified.
      image_ext: The extension of the file being verified (e.g. '.img').
      expected_chain_partitions_map: A map from partition name to the
          tuple (rollback_index_location, key_blob).
      image_containing_descriptor: The image the descriptor is in.
      accept_zeroed_hashtree: If True, don't fail if hashtree or FEC data is
          zeroed out.

    Returns:
      True if the descriptor verifies, False otherwise.
    """
    if not self.partition_name:
      image_filename = image_containing_descriptor.filename
      image = image_containing_descriptor
    else:
      image_filename = os.path.join(image_dir, self.partition_name + image_ext)
      image = ImageHandler(image_filename, read_only=True)
    # Generate the hashtree and checks that it matches what's in the file.
    digest_size = self._hashtree_digest_size()
    digest_padding = round_to_pow2(digest_size) - digest_size
    (hash_level_offsets, tree_size) = calc_hash_level_offsets(
        self.image_size, self.data_block_size, digest_size + digest_padding)
    root_digest, hash_tree = generate_hash_tree(image, self.image_size,
                                                self.data_block_size,
                                                self.hash_algorithm, self.salt,
                                                digest_padding,
                                                hash_level_offsets,
                                                tree_size)
    # The root digest must match unless it is not embedded in the descriptor.
    if self.root_digest and root_digest != self.root_digest:
      sys.stderr.write('hashtree of {} does not match descriptor\n'.
                       format(image_filename))
      return False
    # ... also check that the on-disk hashtree matches
    image.seek(self.tree_offset)
    hash_tree_ondisk = image.read(self.tree_size)
    is_zeroed = (self.tree_size == 0) or (hash_tree_ondisk[0:8] == b'ZeRoHaSH')
    if is_zeroed and accept_zeroed_hashtree:
      print('{}: skipping verification since hashtree is zeroed and '
            '--accept_zeroed_hashtree was given'
            .format(self.partition_name))
    else:
      if hash_tree != hash_tree_ondisk:
        sys.stderr.write('hashtree of {} contains invalid data\n'.
                         format(image_filename))
        return False
      print('{}: Successfully verified {} hashtree of {} for image of {} bytes'
            .format(self.partition_name, self.hash_algorithm, image.filename,
                    self.image_size))
    # TODO(zeuthen): we could also verify that the FEC stored in the image is
    # correct but this a) currently requires the 'fec' binary; and b) takes a
    # long time; and c) is not strictly needed for verification purposes as
    # we've already verified the root hash.
    return True


class AvbHashDescriptor(AvbDescriptor):
  """A class for hash descriptors.

  See the |AvbHashDescriptor| C struct for more information.

  Attributes:
    image_size: Image size, in bytes.
    hash_algorithm: Hash algorithm used as string.
    partition_name: Partition name as string.
    salt: Salt used as bytes.
    digest: The hash value of salt and data combined as bytes.
    flags: The descriptor flags (see avb_hash_descriptor.h).
  """

  TAG = 2
  RESERVED = 60
  SIZE = 72 + RESERVED
  FORMAT_STRING = ('!QQ'  # tag, num_bytes_following (descriptor header)
                   'Q'    # image size (bytes)
                   '32s'  # hash algorithm used
                   'L'    # partition name (bytes)
                   'L'    # salt length (bytes)
                   'L'    # digest length (bytes)
                   'L' +  # flags
                   str(RESERVED) + 's')  # reserved

  def __init__(self, data=None):
    """Initializes a new hash descriptor.

    Arguments:
      data: If not None, must be bytes of size |SIZE|.

    Raises:
      LookupError: If the given descriptor is malformed.
    """
    super().__init__(None)
    assert struct.calcsize(self.FORMAT_STRING) == self.SIZE

    if data:
      (tag, num_bytes_following, self.image_size, self.hash_algorithm,
       partition_name_len, salt_len,
       digest_len, self.flags, _) = struct.unpack(self.FORMAT_STRING,
                                                  data[0:self.SIZE])
      expected_size = round_to_multiple(
          self.SIZE - 16 + partition_name_len + salt_len + digest_len, 8)
      if tag != self.TAG or num_bytes_following != expected_size:
        raise LookupError('Given data does not look like a hash descriptor.')
      # Nuke NUL-bytes at the end.
      self.hash_algorithm = self.hash_algorithm.rstrip(b'\0').decode('ascii')
      o = 0
      try:
        self.partition_name = data[
            (self.SIZE + o):(self.SIZE + o + partition_name_len)
        ].decode('utf-8')
      except UnicodeDecodeError as e:
        raise LookupError('Partition name cannot be decoded as UTF-8: {}.'
                          .format(e)) from e
      o += partition_name_len
      self.salt = data[(self.SIZE + o):(self.SIZE + o + salt_len)]
      o += salt_len
      self.digest = data[(self.SIZE + o):(self.SIZE + o + digest_len)]
      if digest_len != len(hashlib.new(self.hash_algorithm).digest()):
        if digest_len != 0:
          raise LookupError('digest_len doesn\'t match hash algorithm')

    else:
      self.image_size = 0
      self.hash_algorithm = ''
      self.partition_name = ''
      self.salt = b''
      self.digest = b''
      self.flags = 0

  def print_desc(self, o):
    """Print the descriptor.

    Arguments:
      o: The object to write the output to.
    """
    o.write('    Hash descriptor:\n')
    o.write('      Image Size:            {} bytes\n'.format(self.image_size))
    o.write('      Hash Algorithm:        {}\n'.format(self.hash_algorithm))
    o.write('      Partition Name:        {}\n'.format(self.partition_name))
    o.write('      Salt:                  {}\n'.format(self.salt.hex()))
    o.write('      Digest:                {}\n'.format(self.digest.hex()))
    o.write('      Flags:                 {}\n'.format(self.flags))

  def encode(self):
    """Serializes the descriptor.

    Returns:
      The descriptor data as bytes.
    """
    hash_algorithm_encoded = self.hash_algorithm.encode('ascii')
    partition_name_encoded = self.partition_name.encode('utf-8')
    num_bytes_following = (self.SIZE + len(partition_name_encoded) +
                           len(self.salt) + len(self.digest) - 16)
    nbf_with_padding = round_to_multiple(num_bytes_following, 8)
    padding_size = nbf_with_padding - num_bytes_following
    desc = struct.pack(self.FORMAT_STRING, self.TAG, nbf_with_padding,
                       self.image_size, hash_algorithm_encoded,
                       len(partition_name_encoded), len(self.salt),
                       len(self.digest), self.flags, self.RESERVED * b'\0')
    ret = (desc + partition_name_encoded + self.salt + self.digest +
           padding_size * b'\0')
    return ret

  def verify(self, image_dir, image_ext, expected_chain_partitions_map,
             image_containing_descriptor, accept_zeroed_hashtree):
    """Verifies contents of the descriptor - used in verify_image sub-command.

    Arguments:
      image_dir: The directory of the file being verified.
      image_ext: The extension of the file being verified (e.g. '.img').
      expected_chain_partitions_map: A map from partition name to the
          tuple (rollback_index_location, key_blob).
      image_containing_descriptor: The image the descriptor is in.
      accept_zeroed_hashtree: If True, don't fail if hashtree or FEC data is
          zeroed out.

    Returns:
      True if the descriptor verifies, False otherwise.
    """
    if not self.partition_name:
      image_filename = image_containing_descriptor.filename
      image = image_containing_descriptor
    else:
      image_filename = os.path.join(image_dir, self.partition_name + image_ext)
      image = ImageHandler(image_filename, read_only=True)
    data = image.read(self.image_size)
    ha = hashlib.new(self.hash_algorithm)
    ha.update(self.salt)
    ha.update(data)
    digest = ha.digest()
    # The digest must match unless there is no digest in the descriptor.
    if self.digest and digest != self.digest:
      sys.stderr.write('{} digest of {} does not match digest in descriptor\n'.
                       format(self.hash_algorithm, image_filename))
      return False
    print('{}: Successfully verified {} hash of {} for image of {} bytes'
          .format(self.partition_name, self.hash_algorithm, image.filename,
                  self.image_size))
    return True


class AvbKernelCmdlineDescriptor(AvbDescriptor):
  """A class for kernel command-line descriptors.

  See the |AvbKernelCmdlineDescriptor| C struct for more information.

  Attributes:
    flags: Flags.
    kernel_cmdline: The kernel command-line as string.
  """

  TAG = 3
  SIZE = 24
  FORMAT_STRING = ('!QQ'  # tag, num_bytes_following (descriptor header)
                   'L'    # flags
                   'L')   # cmdline length (bytes)

  FLAGS_USE_ONLY_IF_HASHTREE_NOT_DISABLED = (1 << 0)
  FLAGS_USE_ONLY_IF_HASHTREE_DISABLED = (1 << 1)

  def __init__(self, data=None):
    """Initializes a new kernel cmdline descriptor.

    Arguments:
      data: If not None, must be bytes of size |SIZE|.

    Raises:
      LookupError: If the given descriptor is malformed.
    """
    super().__init__(None)
    assert struct.calcsize(self.FORMAT_STRING) == self.SIZE

    if data:
      (tag, num_bytes_following, self.flags, kernel_cmdline_length) = (
          struct.unpack(self.FORMAT_STRING, data[0:self.SIZE]))
      expected_size = round_to_multiple(self.SIZE - 16 + kernel_cmdline_length,
                                        8)
      if tag != self.TAG or num_bytes_following != expected_size:
        raise LookupError('Given data does not look like a kernel cmdline '
                          'descriptor.')
      # Nuke NUL-bytes at the end.
      try:
        self.kernel_cmdline = data[
            self.SIZE:(self.SIZE + kernel_cmdline_length)].decode('utf-8')
      except UnicodeDecodeError as e:
        raise LookupError('Kernel command-line cannot be decoded as UTF-8: {}.'
                          .format(e)) from e
    else:
      self.flags = 0
      self.kernel_cmdline = ''

  def print_desc(self, o):
    """Print the descriptor.

    Arguments:
      o: The object to write the output to.
    """
    o.write('    Kernel Cmdline descriptor:\n')
    o.write('      Flags:                 {}\n'.format(self.flags))
    o.write('      Kernel Cmdline:        \'{}\'\n'.format(self.kernel_cmdline))

  def encode(self):
    """Serializes the descriptor.

    Returns:
      The descriptor data as bytes.
    """
    kernel_cmd_encoded = self.kernel_cmdline.encode('utf-8')
    num_bytes_following = (self.SIZE + len(kernel_cmd_encoded) - 16)
    nbf_with_padding = round_to_multiple(num_bytes_following, 8)
    padding_size = nbf_with_padding - num_bytes_following
    desc = struct.pack(self.FORMAT_STRING, self.TAG, nbf_with_padding,
                       self.flags, len(kernel_cmd_encoded))
    ret = desc + kernel_cmd_encoded + padding_size * b'\0'
    return ret

  def verify(self, image_dir, image_ext, expected_chain_partitions_map,
             image_containing_descriptor, accept_zeroed_hashtree):
    """Verifies contents of the descriptor - used in verify_image sub-command.

    Arguments:
      image_dir: The directory of the file being verified.
      image_ext: The extension of the file being verified (e.g. '.img').
      expected_chain_partitions_map: A map from partition name to the
          tuple (rollback_index_location, key_blob).
      image_containing_descriptor: The image the descriptor is in.
      accept_zeroed_hashtree: If True, don't fail if hashtree or FEC data is
          zeroed out.

    Returns:
      True if the descriptor verifies, False otherwise.
    """
    # Nothing to verify.
    return True


class AvbChainPartitionDescriptor(AvbDescriptor):
  """A class for chained partition descriptors.

  See the |AvbChainPartitionDescriptor| C struct for more information.

  Attributes:
    rollback_index_location: The rollback index location to use.
    partition_name: Partition name as string.
    public_key: The public key as bytes.
    flags: Descriptor flags (see avb_chain_partition_descriptor.h).
  """

  TAG = 4
  RESERVED = 60
  SIZE = 32 + RESERVED
  FORMAT_STRING = ('!QQ'  # tag, num_bytes_following (descriptor header)
                   'L'    # rollback_index_location
                   'L'    # partition_name_size (bytes)
                   'L' +  # public_key_size (bytes)
                   'L' +  # flags
                   str(RESERVED) + 's')  # reserved

  def __init__(self, data=None):
    """Initializes a new chain partition descriptor.

    Arguments:
      data: If not None, must be a bytearray of size |SIZE|.

    Raises:
      LookupError: If the given descriptor is malformed.
    """
    AvbDescriptor.__init__(self, None)
    assert struct.calcsize(self.FORMAT_STRING) == self.SIZE

    if data:
      (tag, num_bytes_following, self.rollback_index_location,
       partition_name_len,
       public_key_len, self.flags, _) = struct.unpack(self.FORMAT_STRING,
                                                      data[0:self.SIZE])
      expected_size = round_to_multiple(
          self.SIZE - 16 + partition_name_len + public_key_len, 8)
      if tag != self.TAG or num_bytes_following != expected_size:
        raise LookupError('Given data does not look like a chain partition '
                          'descriptor.')
      o = 0
      try:
        self.partition_name = data[
            (self.SIZE + o):(self.SIZE + o + partition_name_len)
        ].decode('utf-8')
      except UnicodeDecodeError as e:
        raise LookupError('Partition name cannot be decoded as UTF-8: {}.'
                          .format(e)) from e
      o += partition_name_len
      self.public_key = data[(self.SIZE + o):(self.SIZE + o + public_key_len)]

    else:
      self.rollback_index_location = 0
      self.partition_name = ''
      self.public_key = b''
      self.flags = 0

  def print_desc(self, o):
    """Print the descriptor.

    Arguments:
      o: The object to write the output to.
    """
    o.write('    Chain Partition descriptor:\n')
    o.write('      Partition Name:          {}\n'.format(self.partition_name))
    o.write('      Rollback Index Location: {}\n'.format(
        self.rollback_index_location))
    # Just show the SHA1 of the key, for size reasons.
    pubkey_digest = hashlib.sha1(self.public_key).hexdigest()
    o.write('      Public key (sha1):       {}\n'.format(pubkey_digest))
    o.write('      Flags:                   {}\n'.format(self.flags))

  def encode(self):
    """Serializes the descriptor.

    Returns:
      The descriptor data as bytes.
    """
    partition_name_encoded = self.partition_name.encode('utf-8')
    num_bytes_following = (
        self.SIZE + len(partition_name_encoded) + len(self.public_key) - 16)
    nbf_with_padding = round_to_multiple(num_bytes_following, 8)
    padding_size = nbf_with_padding - num_bytes_following
    desc = struct.pack(self.FORMAT_STRING, self.TAG, nbf_with_padding,
                       self.rollback_index_location,
                       len(partition_name_encoded), len(self.public_key),
                       self.flags, self.RESERVED * b'\0')
    ret = desc + partition_name_encoded + self.public_key + padding_size * b'\0'
    return ret

  def verify(self, image_dir, image_ext, expected_chain_partitions_map,
             image_containing_descriptor, accept_zeroed_hashtree):
    """Verifies contents of the descriptor - used in verify_image sub-command.

    Arguments:
      image_dir: The directory of the file being verified.
      image_ext: The extension of the file being verified (e.g. '.img').
      expected_chain_partitions_map: A map from partition name to the
          tuple (rollback_index_location, key_blob).
      image_containing_descriptor: The image the descriptor is in.
      accept_zeroed_hashtree: If True, don't fail if hashtree or FEC data is
          zeroed out.

    Returns:
      True if the descriptor verifies, False otherwise.
    """
    value = expected_chain_partitions_map.get(self.partition_name)
    if not value:
      sys.stderr.write('No expected chain partition for partition {}. Use '
                       '--expected_chain_partition to specify expected '
                       'contents or --follow_chain_partitions.\n'.
                       format(self.partition_name))
      return False
    rollback_index_location, pk_blob = value

    if self.rollback_index_location != rollback_index_location:
      sys.stderr.write('Expected rollback_index_location {} does not '
                       'match {} in descriptor for partition {}\n'.
                       format(rollback_index_location,
                              self.rollback_index_location,
                              self.partition_name))
      return False

    if self.public_key != pk_blob:
      sys.stderr.write('Expected public key blob does not match public '
                       'key blob in descriptor for partition {}\n'.
                       format(self.partition_name))
      return False

    print('{}: Successfully verified chain partition descriptor matches '
          'expected data'.format(self.partition_name))

    return True

DESCRIPTOR_CLASSES = [
    AvbPropertyDescriptor, AvbHashtreeDescriptor, AvbHashDescriptor,
    AvbKernelCmdlineDescriptor, AvbChainPartitionDescriptor
]


def parse_descriptors(data):
  """Parses a blob of data into descriptors.

  Arguments:
    data: Encoded descriptors as bytes.

  Returns:
    A list of instances of objects derived from AvbDescriptor. For
    unknown descriptors, the class AvbDescriptor is used.
  """
  o = 0
  ret = []
  while o < len(data):
    tag, nb_following = struct.unpack('!2Q', data[o:o + 16])
    if tag < len(DESCRIPTOR_CLASSES):
      clazz = DESCRIPTOR_CLASSES[tag]
    else:
      clazz = AvbDescriptor
    ret.append(clazz(data[o:o + 16 + nb_following]))
    o += 16 + nb_following
  return ret


class AvbFooter(object):
  """A class for parsing and writing footers.

  Footers are stored at the end of partitions and point to where the
  AvbVBMeta blob is located. They also contain the original size of
  the image before AVB information was added.

  Attributes:
    magic: Magic for identifying the footer, see |MAGIC|.
    version_major: The major version of avbtool that wrote the footer.
    version_minor: The minor version of avbtool that wrote the footer.
    original_image_size: Original image size.
    vbmeta_offset: Offset of where the AvbVBMeta blob is stored.
    vbmeta_size: Size of the AvbVBMeta blob.
  """

  MAGIC = b'AVBf'
  SIZE = 64
  RESERVED = 28
  FOOTER_VERSION_MAJOR = AVB_FOOTER_VERSION_MAJOR
  FOOTER_VERSION_MINOR = AVB_FOOTER_VERSION_MINOR
  FORMAT_STRING = ('!4s2L'  # magic, 2 x version.
                   'Q'      # Original image size.
                   'Q'      # Offset of VBMeta blob.
                   'Q' +    # Size of VBMeta blob.
                   str(RESERVED) + 'x')  # padding for reserved bytes

  def __init__(self, data=None):
    """Initializes a new footer object.

    Arguments:
      data: If not None, must be bytes of size 4096.

    Raises:
      LookupError: If the given footer is malformed.
      struct.error: If the given data has no footer.
    """
    assert struct.calcsize(self.FORMAT_STRING) == self.SIZE

    if data:
      (self.magic, self.version_major, self.version_minor,
       self.original_image_size, self.vbmeta_offset,
       self.vbmeta_size) = struct.unpack(self.FORMAT_STRING, data)
      if self.magic != self.MAGIC:
        raise LookupError('Given data does not look like a AVB footer.')
    else:
      self.magic = self.MAGIC
      self.version_major = self.FOOTER_VERSION_MAJOR
      self.version_minor = self.FOOTER_VERSION_MINOR
      self.original_image_size = 0
      self.vbmeta_offset = 0
      self.vbmeta_size = 0

  def encode(self):
    """Serializes the footer.

    Returns:
      The footer as bytes.
    """
    return struct.pack(self.FORMAT_STRING, self.magic, self.version_major,
                       self.version_minor, self.original_image_size,
                       self.vbmeta_offset, self.vbmeta_size)


class AvbVBMetaHeader(object):
  """A class for parsing and writing AVB vbmeta images.

  The attributes correspond to the |AvbVBMetaImageHeader| struct defined in
  avb_vbmeta_image.h.

  Attributes:
    magic: Four bytes equal to "AVB0" (AVB_MAGIC).
    required_libavb_version_major: The major version of libavb required for this
        header.
    required_libavb_version_minor: The minor version of libavb required for this
        header.
    authentication_data_block_size: The size of the signature block.
    auxiliary_data_block_size: The size of the auxiliary data block.
    algorithm_type: The verification algorithm used, see |AvbAlgorithmType|
        enum.
    hash_offset: Offset into the "Authentication data" block of hash data.
    hash_size: Length of the hash data.
    signature_offset: Offset into the "Authentication data" block of signature
        data.
    signature_size: Length of the signature data.
    public_key_offset: Offset into the "Auxiliary data" block of public key
        data.
    public_key_size: Length of the public key data.
    public_key_metadata_offset: Offset into the "Auxiliary data" block of public
        key metadata.
    public_key_metadata_size: Length of the public key metadata. Must be set to
        zero if there is no public key metadata.
    descriptors_offset: Offset into the "Auxiliary data" block of descriptor
        data.
    descriptors_size: Length of descriptor data.
    rollback_index: The rollback index which can be used to prevent rollback to
        older versions.
    flags: Flags from the AvbVBMetaImageFlags enumeration. This must be set to
        zero if the vbmeta image is not a top-level image.
    rollback_index_location: The location of the rollback index defined in this
        header. Only valid for the main vbmeta. For chained partitions, the
        rollback index location must be specified in the
        AvbChainPartitionDescriptor and this value must be set to 0.
    release_string: The release string from avbtool, e.g. "avbtool 1.0.0" or
        "avbtool 1.0.0 xyz_board Git-234abde89". Is guaranteed to be NUL
        terminated. Applications must not make assumptions about how this
        string is formatted.
  """
  MAGIC = b'AVB0'
  SIZE = 256

  # Keep in sync with |reserved| field of |AvbVBMetaImageHeader|.
  RESERVED = 80

  # Keep in sync with |AvbVBMetaImageHeader|.
  FORMAT_STRING = ('!4s2L'   # magic, 2 x version
                   '2Q'      # 2 x block size
                   'L'       # algorithm type
                   '2Q'      # offset, size (hash)
                   '2Q'      # offset, size (signature)
                   '2Q'      # offset, size (public key)
                   '2Q'      # offset, size (public key metadata)
                   '2Q'      # offset, size (descriptors)
                   'Q'       # rollback_index
                   'L'       # flags
                   'L'       # rollback_index_location
                   '47sx' +  # NUL-terminated release string
                   str(RESERVED) + 'x')  # padding for reserved bytes

  def __init__(self, data=None):
    """Initializes a new header object.

    Arguments:
      data: If not None, must be a bytearray of size 8192.

    Raises:
      Exception: If the given data is malformed.
    """
    assert struct.calcsize(self.FORMAT_STRING) == self.SIZE

    if data:
      (self.magic, self.required_libavb_version_major,
       self.required_libavb_version_minor,
       self.authentication_data_block_size, self.auxiliary_data_block_size,
       self.algorithm_type, self.hash_offset, self.hash_size,
       self.signature_offset, self.signature_size, self.public_key_offset,
       self.public_key_size, self.public_key_metadata_offset,
       self.public_key_metadata_size, self.descriptors_offset,
       self.descriptors_size,
       self.rollback_index,
       self.flags,
       self.rollback_index_location,
       release_string) = struct.unpack(self.FORMAT_STRING, data)
      # Nuke NUL-bytes at the end of the string.
      if self.magic != self.MAGIC:
        raise AvbError('Given image does not look like a vbmeta image.')
      self.release_string = release_string.rstrip(b'\0').decode('utf-8')
    else:
      self.magic = self.MAGIC
      # Start by just requiring version 1.0. Code that adds features
      # in a future version can use bump_required_libavb_version_minor() to
      # bump the minor.
      self.required_libavb_version_major = AVB_VERSION_MAJOR
      self.required_libavb_version_minor = 0
      self.authentication_data_block_size = 0
      self.auxiliary_data_block_size = 0
      self.algorithm_type = 0
      self.hash_offset = 0
      self.hash_size = 0
      self.signature_offset = 0
      self.signature_size = 0
      self.public_key_offset = 0
      self.public_key_size = 0
      self.public_key_metadata_offset = 0
      self.public_key_metadata_size = 0
      self.descriptors_offset = 0
      self.descriptors_size = 0
      self.rollback_index = 0
      self.flags = 0
      self.rollback_index_location = 0
      self.release_string = get_release_string()

  def bump_required_libavb_version_minor(self, minor):
    """Function to bump required_libavb_version_minor.

    Call this when writing data that requires a specific libavb
    version to parse it.

    Arguments:
      minor: The minor version of libavb that has support for the feature.
    """
    self.required_libavb_version_minor = (
        max(self.required_libavb_version_minor, minor))

  def encode(self):
    """Serializes the header.

    Returns:
      The header as bytes.
    """
    release_string_encoded = self.release_string.encode('utf-8')
    return struct.pack(self.FORMAT_STRING, self.magic,
                       self.required_libavb_version_major,
                       self.required_libavb_version_minor,
                       self.authentication_data_block_size,
                       self.auxiliary_data_block_size, self.algorithm_type,
                       self.hash_offset, self.hash_size, self.signature_offset,
                       self.signature_size, self.public_key_offset,
                       self.public_key_size, self.public_key_metadata_offset,
                       self.public_key_metadata_size, self.descriptors_offset,
                       self.descriptors_size, self.rollback_index, self.flags,
                       self.rollback_index_location, release_string_encoded)


class Avb(object):
  """Business logic for avbtool command-line tool."""

  # Keep in sync with avb_ab_flow.h.
  AB_FORMAT_NO_CRC = '!4sBB2xBBBxBBBx12x'
  AB_MAGIC = b'\0AB0'
  AB_MAJOR_VERSION = 1
  AB_MINOR_VERSION = 0
  AB_MISC_METADATA_OFFSET = 2048

  # Constants for maximum metadata size. These are used to give
  # meaningful errors if the value passed in via --partition_size is
  # too small and when --calc_max_image_size is used. We use
  # conservative figures.
  MAX_VBMETA_SIZE = 64 * 1024
  MAX_FOOTER_SIZE = 4096

  def generate_test_image(self, output, image_size, start_byte):
    """Generates a test image for testing avbtool with known content.

    The content has following pattern: 0x00 0x01 0x02 .. 0xff 0x00 0x01 ..).

    Arguments:
      output: Write test image to this file.
      image_size: The size of the requested file in bytes.
      start_byte: The integer value of the start byte to use for pattern
          generation.
    """
    pattern = bytearray([x & 0xFF for x in range(start_byte, start_byte + 256)])
    buf = bytearray()
    c = int(math.ceil(image_size / 256.0))
    for _ in range(0, c):
      buf.extend(pattern)
    output.write(buf[0:image_size])

  def extract_vbmeta_image(self, output, image_filename, padding_size):
    """Implements the 'extract_vbmeta_image' command.

    Arguments:
      output: Write vbmeta struct to this file.
      image_filename: File to extract vbmeta data from (with a footer).
      padding_size: If not 0, pads output so size is a multiple of the number.

    Raises:
      AvbError: If there's no footer in the image.
    """
    image = ImageHandler(image_filename, read_only=True)
    (footer, _, _, _) = self._parse_image(image)
    if not footer:
      raise AvbError('Given image does not have a footer.')

    image.seek(footer.vbmeta_offset)
    vbmeta_blob = image.read(footer.vbmeta_size)
    output.write(vbmeta_blob)

    if padding_size > 0:
      padded_size = round_to_multiple(len(vbmeta_blob), padding_size)
      padding_needed = padded_size - len(vbmeta_blob)
      output.write(b'\0' * padding_needed)

  def erase_footer(self, image_filename, keep_hashtree):
    """Implements the 'erase_footer' command.

    Arguments:
      image_filename: File to erase a footer from.
      keep_hashtree: If True, keep the hashtree and FEC around.

    Raises:
      AvbError: If there's no footer in the image.
    """
    image = ImageHandler(image_filename)
    (footer, _, descriptors, _) = self._parse_image(image)
    if not footer:
      raise AvbError('Given image does not have a footer.')

    new_image_size = None
    if not keep_hashtree:
      new_image_size = footer.original_image_size
    else:
      # If requested to keep the hashtree, search for a hashtree
      # descriptor to figure out the location and size of the hashtree
      # and FEC.
      for desc in descriptors:
        if isinstance(desc, AvbHashtreeDescriptor):
          # The hashtree is always just following the main data so the
          # new size is easily derived.
          new_image_size = desc.tree_offset + desc.tree_size
          # If the image has FEC codes, also keep those.
          if desc.fec_offset > 0:
            fec_end = desc.fec_offset + desc.fec_size
            new_image_size = max(new_image_size, fec_end)
          break
      if not new_image_size:
        raise AvbError('Requested to keep hashtree but no hashtree '
                       'descriptor was found.')

    # And cut...
    image.truncate(new_image_size)

  def zero_hashtree(self, image_filename):
    """Implements the 'zero_hashtree' command.

    Arguments:
      image_filename: File to zero hashtree and FEC data from.

    Raises:
      AvbError: If there's no footer in the image.
    """
    image = ImageHandler(image_filename)
    (footer, _, descriptors, _) = self._parse_image(image)
    if not footer:
      raise AvbError('Given image does not have a footer.')

    # Search for a hashtree descriptor to figure out the location and
    # size of the hashtree and FEC.
    ht_desc = None
    for desc in descriptors:
      if isinstance(desc, AvbHashtreeDescriptor):
        ht_desc = desc
        break

    if not ht_desc:
      raise AvbError('No hashtree descriptor was found.')

    zero_ht_start_offset = ht_desc.tree_offset
    zero_ht_num_bytes = ht_desc.tree_size
    zero_fec_start_offset = None
    zero_fec_num_bytes = 0
    if ht_desc.fec_offset > 0:
      if ht_desc.fec_offset != ht_desc.tree_offset + ht_desc.tree_size:
        raise AvbError('Hash-tree and FEC data must be adjacent.')
      zero_fec_start_offset = ht_desc.fec_offset
      zero_fec_num_bytes = ht_desc.fec_size
    zero_end_offset = (zero_ht_start_offset + zero_ht_num_bytes
                       + zero_fec_num_bytes)
    image.seek(zero_end_offset)
    data = image.read(image.image_size - zero_end_offset)

    # Write zeroes all over hashtree and FEC, except for the first eight bytes
    # where a magic marker - ZeroHaSH - is placed. Place these markers in the
    # beginning of both hashtree and FEC. (That way, in the future we can add
    # options to 'avbtool zero_hashtree' so as to zero out only either/or.)
    #
    # Applications can use these markers to detect that the hashtree and/or
    # FEC needs to be recomputed.
    image.truncate(zero_ht_start_offset)
    data_zeroed_firstblock = b'ZeRoHaSH' + b'\0' * (image.block_size - 8)
    image.append_raw(data_zeroed_firstblock)
    image.append_fill(b'\0\0\0\0', zero_ht_num_bytes - image.block_size)
    if zero_fec_start_offset:
      image.append_raw(data_zeroed_firstblock)
      image.append_fill(b'\0\0\0\0', zero_fec_num_bytes - image.block_size)
    image.append_raw(data)

  def resize_image(self, image_filename, partition_size):
    """Implements the 'resize_image' command.

    Arguments:
      image_filename: File with footer to resize.
      partition_size: The new size of the image.

    Raises:
      AvbError: If there's no footer in the image.
    """

    image = ImageHandler(image_filename)
    if partition_size % image.block_size != 0:
      raise AvbError('Partition size of {} is not a multiple of the image '
                     'block size {}.'.format(partition_size,
                                             image.block_size))
    (footer, _, _, _) = self._parse_image(image)
    if not footer:
      raise AvbError('Given image does not have a footer.')

    # The vbmeta blob is always at the end of the data so resizing an
    # image amounts to just moving the footer around.
    vbmeta_end_offset = footer.vbmeta_offset + footer.vbmeta_size
    if vbmeta_end_offset % image.block_size != 0:
      vbmeta_end_offset += image.block_size - (vbmeta_end_offset
                                               % image.block_size)

    if partition_size < vbmeta_end_offset + 1 * image.block_size:
      raise AvbError('Requested size of {} is too small for an image '
                     'of size {}.'
                     .format(partition_size,
                             vbmeta_end_offset + 1 * image.block_size))

    # Cut at the end of the vbmeta blob and insert a DONT_CARE chunk
    # with enough bytes such that the final Footer block is at the end
    # of partition_size.
    image.truncate(vbmeta_end_offset)
    image.append_dont_care(partition_size - vbmeta_end_offset -
                           1 * image.block_size)

    # Just reuse the same footer - only difference is that we're
    # writing it in a different place.
    footer_blob = footer.encode()
    footer_blob_with_padding = (b'\0' * (image.block_size - AvbFooter.SIZE) +
                                footer_blob)
    image.append_raw(footer_blob_with_padding)

  def set_ab_metadata(self, misc_image, slot_data):
    """Implements the 'set_ab_metadata' command.

    The |slot_data| argument must be of the form 'A_priority:A_tries_remaining:
    A_successful_boot:B_priority:B_tries_remaining:B_successful_boot'.

    Arguments:
      misc_image: The misc image to write to.
      slot_data: Slot data as a string

    Raises:
      AvbError: If slot data is malformed.
    """
    tokens = slot_data.split(':')
    if len(tokens) != 6:
      raise AvbError('Malformed slot data "{}".'.format(slot_data))
    a_priority = int(tokens[0])
    a_tries_remaining = int(tokens[1])
    a_success = int(tokens[2]) != 0
    b_priority = int(tokens[3])
    b_tries_remaining = int(tokens[4])
    b_success = int(tokens[5]) != 0

    ab_data_no_crc = struct.pack(self.AB_FORMAT_NO_CRC,
                                 self.AB_MAGIC,
                                 self.AB_MAJOR_VERSION, self.AB_MINOR_VERSION,
                                 a_priority, a_tries_remaining, a_success,
                                 b_priority, b_tries_remaining, b_success)
    # Force CRC to be unsigned, see https://bugs.python.org/issue4903 for why.
    crc_value = binascii.crc32(ab_data_no_crc) & 0xffffffff
    ab_data = ab_data_no_crc + struct.pack('!I', crc_value)
    misc_image.seek(self.AB_MISC_METADATA_OFFSET)
    misc_image.write(ab_data)

  def info_image(self, image_filename, output, cert, output_pubkey=None):
    """Implements the 'info_image' command.

    Arguments:
      image_filename: Image file to get information from (file object).
      output: Output file to write human-readable information to (file object).
      cert: If True, show information about the avb_cert certificates.
      output_pubkey: Optional file to write the public key to (file object).
    """
    image = ImageHandler(image_filename, read_only=True)
    o = output
    (footer, header, descriptors, image_size) = self._parse_image(image)

    # To show the SHA1 of the public key.
    vbmeta_blob = self._load_vbmeta_blob(image)
    key_offset = (header.SIZE +
                  header.authentication_data_block_size +
                  header.public_key_offset)
    key_blob = vbmeta_blob[key_offset:key_offset + header.public_key_size]

    if footer:
      o.write('Footer version:           {}.{}\n'.format(footer.version_major,
                                                         footer.version_minor))
      o.write('Image size:               {} bytes\n'.format(image_size))
      o.write('Original image size:      {} bytes\n'.format(
          footer.original_image_size))
      o.write('VBMeta offset:            {}\n'.format(footer.vbmeta_offset))
      o.write('VBMeta size:              {} bytes\n'.format(footer.vbmeta_size))
      o.write('--\n')

    (alg_name, _) = lookup_algorithm_by_type(header.algorithm_type)

    o.write('Minimum libavb version:   {}.{}{}\n'.format(
        header.required_libavb_version_major,
        header.required_libavb_version_minor,
        ' (Sparse)' if image.is_sparse else ''))
    o.write('Header Block:             {} bytes\n'.format(AvbVBMetaHeader.SIZE))
    o.write('Authentication Block:     {} bytes\n'.format(
        header.authentication_data_block_size))
    o.write('Auxiliary Block:          {} bytes\n'.format(
        header.auxiliary_data_block_size))
    if key_blob:
      hexdig = hashlib.sha1(key_blob).hexdigest()
      o.write('Public key (sha1):        {}\n'.format(hexdig))
      if output_pubkey is not None:
        output_pubkey.write(key_blob)

    o.write('Algorithm:                {}\n'.format(alg_name))
    o.write('Rollback Index:           {}\n'.format(header.rollback_index))
    o.write('Flags:                    {}\n'.format(header.flags))
    o.write('Rollback Index Location:  {}\n'.format(
        header.rollback_index_location))
    o.write('Release String:           \'{}\'\n'.format(header.release_string))

    # Print descriptors.
    num_printed = 0
    o.write('Descriptors:\n')
    for desc in descriptors:
      desc.print_desc(o)
      num_printed += 1
    if num_printed == 0:
      o.write('    (none)\n')

    if cert and header.public_key_metadata_size:
      o.write('avb_cert certificate:\n')
      key_metadata_offset = (header.SIZE +
                             header.authentication_data_block_size +
                             header.public_key_metadata_offset)
      key_metadata_blob = vbmeta_blob[key_metadata_offset: key_metadata_offset
                                      + header.public_key_metadata_size]
      version, pik, psk = struct.unpack('<I1620s1620s', key_metadata_blob)
      o.write('    Metadata version:        {}\n'.format(version))

      def print_certificate(cert):
        version, public_key, subject, usage, key_version, _ = (
            struct.unpack('<I1032s32s32sQ512s', cert))
        o.write('      Version:               {}\n'.format(version))
        o.write('      Public key (sha1):     {}\n'.format(
            hashlib.sha1(public_key).hexdigest()))
        o.write('      Subject:               {}\n'.format(subject.hex()))
        o.write('      Usage:                 {}\n'.format(usage.hex()))
        o.write('      Key version:           {}\n'.format(key_version))

      o.write('    Product Intermediate Key:\n')
      print_certificate(pik)
      o.write('    Product Signing Key:\n')
      print_certificate(psk)

  def verify_image(self, image_filename, key_path, expected_chain_partitions,
                   follow_chain_partitions, accept_zeroed_hashtree):
    """Implements the 'verify_image' command.

    Arguments:
      image_filename: Image file to get information from (file object).
      key_path: None or check that embedded public key matches key at given
          path.
      expected_chain_partitions: List of chain partitions to check or None.
      follow_chain_partitions:
          If True, will follows chain partitions even when not specified with
          the --expected_chain_partition option
      accept_zeroed_hashtree: If True, don't fail if hashtree or FEC data is
          zeroed out.

    Raises:
      AvbError: If verification of the image fails.
    """
    expected_chain_partitions_map = {}
    if expected_chain_partitions:
      for cp in expected_chain_partitions:
        cp_tokens = cp.split(':')
        if len(cp_tokens) != 3:
          raise AvbError('Malformed chained partition "{}".'.format(cp))
        partition_name = cp_tokens[0]
        rollback_index_location = int(cp_tokens[1])
        file_path = cp_tokens[2]
        with open(file_path, 'rb') as f:
          pk_blob = f.read()
        expected_chain_partitions_map[partition_name] = (
            rollback_index_location, pk_blob)

    image_dir = os.path.dirname(image_filename)
    image_ext = os.path.splitext(image_filename)[1]

    key_blob = None
    if key_path:
      print('Verifying image {} using key at {}'.format(image_filename,
                                                        key_path))
      key_blob = RSAPublicKey(key_path).encode()
    else:
      print('Verifying image {} using embedded public key'.format(
          image_filename))

    image = ImageHandler(image_filename, read_only=True)
    (footer, header, descriptors, _) = self._parse_image(image)
    offset = 0
    if footer:
      offset = footer.vbmeta_offset

    image.seek(offset)
    vbmeta_blob = image.read(header.SIZE
                             + header.authentication_data_block_size
                             + header.auxiliary_data_block_size)

    alg_name, _ = lookup_algorithm_by_type(header.algorithm_type)
    if not verify_vbmeta_signature(header, vbmeta_blob):
      raise AvbError('Signature check failed for {} vbmeta struct {}'
                     .format(alg_name, image_filename))

    if key_blob:
      # The embedded public key is in the auxiliary block at an offset.
      key_offset = AvbVBMetaHeader.SIZE
      key_offset += header.authentication_data_block_size
      key_offset += header.public_key_offset
      key_blob_in_vbmeta = vbmeta_blob[key_offset:key_offset
                                       + header.public_key_size]
      if key_blob != key_blob_in_vbmeta:
        raise AvbError('Embedded public key does not match given key.')

    if footer:
      print('vbmeta: Successfully verified footer and {} vbmeta struct in {}'
            .format(alg_name, image.filename))
    else:
      print('vbmeta: Successfully verified {} vbmeta struct in {}'
            .format(alg_name, image.filename))

    for desc in descriptors:
      if (isinstance(desc, AvbChainPartitionDescriptor)
          and follow_chain_partitions
          and expected_chain_partitions_map.get(desc.partition_name) is None):
        # In this case we're processing a chain descriptor but don't have a
        # --expect_chain_partition ... however --follow_chain_partitions was
        # specified so we shouldn't error out in desc.verify().
        print('{}: Chained but ROLLBACK_SLOT (which is {}) '
              'and KEY (which has sha1 {}) not specified'
              .format(desc.partition_name, desc.rollback_index_location,
                      hashlib.sha1(desc.public_key).hexdigest()))
      elif not desc.verify(image_dir, image_ext, expected_chain_partitions_map,
                           image, accept_zeroed_hashtree):
        raise AvbError('Error verifying descriptor.')
      # Honor --follow_chain_partitions - add '--' to make the output more
      # readable.
      if (isinstance(desc, AvbChainPartitionDescriptor)
          and follow_chain_partitions):
        print('--')
        chained_image_filename = os.path.join(image_dir,
                                              desc.partition_name + image_ext)
        self.verify_image(chained_image_filename, key_path, None, False,
                          accept_zeroed_hashtree)

  def print_partition_digests(self, image_filename, output, as_json):
    """Implements the 'print_partition_digests' command.

    Arguments:
      image_filename: Image file to get information from (file object).
      output: Output file to write human-readable information to (file object).
      as_json: If True, print information as JSON

    Raises:
      AvbError: If getting the partition digests from the image fails.
    """
    image_dir = os.path.dirname(image_filename)
    image_ext = os.path.splitext(image_filename)[1]
    json_partitions = None
    if as_json:
      json_partitions = []
    self._print_partition_digests(
        image_filename, output, json_partitions, image_dir, image_ext)
    if as_json:
      output.write(json.dumps({'partitions': json_partitions}, indent=2))

  def _print_partition_digests(self, image_filename, output, json_partitions,
                               image_dir, image_ext):
    """Helper for printing partitions.

    Arguments:
      image_filename: Image file to get information from (file object).
      output: Output file to write human-readable information to (file object).
      json_partitions: If not None, don't print to output, instead add partition
          information to this list.
      image_dir: The directory to use when looking for chained partition files.
      image_ext: The extension to use for chained partition files.

    Raises:
      AvbError: If getting the partition digests from the image fails.
    """
    image = ImageHandler(image_filename, read_only=True)
    (_, _, descriptors, _) = self._parse_image(image)

    for desc in descriptors:
      if isinstance(desc, AvbHashDescriptor):
        digest = desc.digest.hex()
        if json_partitions is not None:
          json_partitions.append({'name': desc.partition_name,
                                  'digest': digest})
        else:
          output.write('{}: {}\n'.format(desc.partition_name, digest))
      elif isinstance(desc, AvbHashtreeDescriptor):
        digest = desc.root_digest.hex()
        if json_partitions is not None:
          json_partitions.append({'name': desc.partition_name,
                                  'digest': digest})
        else:
          output.write('{}: {}\n'.format(desc.partition_name, digest))
      elif isinstance(desc, AvbChainPartitionDescriptor):
        chained_image_filename = os.path.join(image_dir,
                                              desc.partition_name + image_ext)
        self._print_partition_digests(
            chained_image_filename, output, json_partitions, image_dir,
            image_ext)

  def calculate_vbmeta_digest(self, image_filename, hash_algorithm, output):
    """Implements the 'calculate_vbmeta_digest' command.

    Arguments:
      image_filename: Image file to get information from (file object).
      hash_algorithm: Hash algorithm used.
      output: Output file to write human-readable information to (file object).
    """

    image_dir = os.path.dirname(image_filename)
    image_ext = os.path.splitext(image_filename)[1]

    image = ImageHandler(image_filename, read_only=True)
    (footer, header, descriptors, _) = self._parse_image(image)
    offset = 0
    if footer:
      offset = footer.vbmeta_offset
    size = (header.SIZE + header.authentication_data_block_size +
            header.auxiliary_data_block_size)
    image.seek(offset)
    vbmeta_blob = image.read(size)

    hasher = hashlib.new(hash_algorithm)
    hasher.update(vbmeta_blob)

    for desc in descriptors:
      if isinstance(desc, AvbChainPartitionDescriptor):
        ch_image_filename = os.path.join(image_dir,
                                         desc.partition_name + image_ext)
        ch_image = ImageHandler(ch_image_filename, read_only=True)
        (ch_footer, ch_header, _, _) = self._parse_image(ch_image)
        ch_offset = 0
        ch_size = (ch_header.SIZE + ch_header.authentication_data_block_size +
                   ch_header.auxiliary_data_block_size)
        if ch_footer:
          ch_offset = ch_footer.vbmeta_offset
        ch_image.seek(ch_offset)
        ch_vbmeta_blob = ch_image.read(ch_size)
        hasher.update(ch_vbmeta_blob)

    digest = hasher.digest()
    output.write('{}\n'.format(digest.hex()))

  def calculate_kernel_cmdline(self, image_filename, hashtree_disabled, output):
    """Implements the 'calculate_kernel_cmdline' command.

    Arguments:
      image_filename: Image file to get information from (file object).
      hashtree_disabled: If True, returns the cmdline for hashtree disabled.
      output: Output file to write human-readable information to (file object).
    """

    image = ImageHandler(image_filename, read_only=True)
    _, _, descriptors, _ = self._parse_image(image)

    image_dir = os.path.dirname(image_filename)
    image_ext = os.path.splitext(image_filename)[1]

    cmdline_descriptors = []
    for desc in descriptors:
      if isinstance(desc, AvbChainPartitionDescriptor):
        ch_image_filename = os.path.join(image_dir,
                                         desc.partition_name + image_ext)
        ch_image = ImageHandler(ch_image_filename, read_only=True)
        _, _, ch_descriptors, _ = self._parse_image(ch_image)
        for ch_desc in ch_descriptors:
          if isinstance(ch_desc, AvbKernelCmdlineDescriptor):
            cmdline_descriptors.append(ch_desc)
      elif isinstance(desc, AvbKernelCmdlineDescriptor):
        cmdline_descriptors.append(desc)

    kernel_cmdline_snippets = []
    for desc in cmdline_descriptors:
      use_cmdline = True
      if ((desc.flags &
           AvbKernelCmdlineDescriptor.FLAGS_USE_ONLY_IF_HASHTREE_NOT_DISABLED)
          != 0):
        if hashtree_disabled:
          use_cmdline = False
      if (desc.flags &
          AvbKernelCmdlineDescriptor.FLAGS_USE_ONLY_IF_HASHTREE_DISABLED) != 0:
        if not hashtree_disabled:
          use_cmdline = False
      if use_cmdline:
        kernel_cmdline_snippets.append(desc.kernel_cmdline)
    output.write(' '.join(kernel_cmdline_snippets))

  def _parse_image(self, image):
    """Gets information about an image.

    The image can either be a vbmeta or an image with a footer.

    Arguments:
      image: An ImageHandler (vbmeta or footer) with a hashtree descriptor.

    Returns:
      A tuple where the first argument is a AvbFooter (None if there
      is no footer on the image), the second argument is a
      AvbVBMetaHeader, the third argument is a list of
      AvbDescriptor-derived instances, and the fourth argument is the
      size of |image|.

    Raises:
      AvbError: In case the image cannot be parsed.
    """
    assert isinstance(image, ImageHandler)
    footer = None
    image.seek(image.image_size - AvbFooter.SIZE)
    try:
      footer = AvbFooter(image.read(AvbFooter.SIZE))
    except (LookupError, struct.error):
      # Nope, just seek back to the start.
      image.seek(0)

    vbmeta_offset = 0
    if footer:
      vbmeta_offset = footer.vbmeta_offset

    image.seek(vbmeta_offset)
    h = AvbVBMetaHeader(image.read(AvbVBMetaHeader.SIZE))

    auth_block_offset = vbmeta_offset + AvbVBMetaHeader.SIZE
    aux_block_offset = auth_block_offset + h.authentication_data_block_size
    desc_start_offset = aux_block_offset + h.descriptors_offset
    image.seek(desc_start_offset)
    descriptors = parse_descriptors(image.read(h.descriptors_size))

    return footer, h, descriptors, image.image_size

  def _load_vbmeta_blob(self, image):
    """Gets the vbmeta struct and associated sections.

    The image can either be a vbmeta.img or an image with a footer.

    Arguments:
      image: An ImageHandler (vbmeta or footer).

    Returns:
      A blob with the vbmeta struct and other sections.
    """
    assert isinstance(image, ImageHandler)
    footer = None
    image.seek(image.image_size - AvbFooter.SIZE)
    try:
      footer = AvbFooter(image.read(AvbFooter.SIZE))
    except (LookupError, struct.error):
      # Nope, just seek back to the start.
      image.seek(0)

    vbmeta_offset = 0
    if footer:
      vbmeta_offset = footer.vbmeta_offset

    image.seek(vbmeta_offset)
    h = AvbVBMetaHeader(image.read(AvbVBMetaHeader.SIZE))

    image.seek(vbmeta_offset)
    data_size = AvbVBMetaHeader.SIZE
    data_size += h.authentication_data_block_size
    data_size += h.auxiliary_data_block_size
    return image.read(data_size)

  def _get_cmdline_descriptors_for_hashtree_descriptor(self, ht):
    """Generate kernel cmdline descriptors for dm-verity.

    Arguments:
      ht: A AvbHashtreeDescriptor

    Returns:
      A list with two AvbKernelCmdlineDescriptor with dm-verity kernel cmdline
      instructions. There is one for when hashtree is not disabled and one for
      when it is.

    """
    c = 'dm="1 vroot none ro 1,'
    c += '0'                                                # start
    c += ' {}'.format((ht.image_size // 512))               # size (# sectors)
    c += ' verity {}'.format(ht.dm_verity_version)          # type and version
    c += ' PARTUUID=$(ANDROID_SYSTEM_PARTUUID)'             # data_dev
    c += ' PARTUUID=$(ANDROID_SYSTEM_PARTUUID)'             # hash_dev
    c += ' {}'.format(ht.data_block_size)                   # data_block
    c += ' {}'.format(ht.hash_block_size)                   # hash_block
    c += ' {}'.format(ht.image_size // ht.data_block_size)  # #blocks
    c += ' {}'.format(ht.image_size // ht.data_block_size)  # hash_offset
    c += ' {}'.format(ht.hash_algorithm)                    # hash_alg
    c += ' {}'.format(ht.root_digest.hex())                 # root_digest
    c += ' {}'.format(ht.salt.hex())                        # salt
    if ht.fec_num_roots > 0:
      if ht.flags & AvbHashtreeDescriptor.FLAGS_CHECK_AT_MOST_ONCE:
        c += ' 11'  # number of optional args
        c += ' check_at_most_once'
      else:
        c += ' 10'  # number of optional args
      c += ' $(ANDROID_VERITY_MODE)'
      c += ' ignore_zero_blocks'
      c += ' use_fec_from_device PARTUUID=$(ANDROID_SYSTEM_PARTUUID)'
      c += ' fec_roots {}'.format(ht.fec_num_roots)
      # Note that fec_blocks is the size that FEC covers, *not* the
      # size of the FEC data. Since we use FEC for everything up until
      # the FEC data, it's the same as the offset.
      c += ' fec_blocks {}'.format(ht.fec_offset // ht.data_block_size)
      c += ' fec_start {}'.format(ht.fec_offset // ht.data_block_size)
    else:
      if ht.flags & AvbHashtreeDescriptor.FLAGS_CHECK_AT_MOST_ONCE:
        c += ' 3'  # number of optional args
        c += ' check_at_most_once'
      else:
        c += ' 2'  # number of optional args
      c += ' $(ANDROID_VERITY_MODE)'
      c += ' ignore_zero_blocks'
    c += '" root=/dev/dm-0'

    # Now that we have the command-line, generate the descriptor.
    desc = AvbKernelCmdlineDescriptor()
    desc.kernel_cmdline = c
    desc.flags = (
        AvbKernelCmdlineDescriptor.FLAGS_USE_ONLY_IF_HASHTREE_NOT_DISABLED)

    # The descriptor for when hashtree verification is disabled is a lot
    # simpler - we just set the root to the partition.
    desc_no_ht = AvbKernelCmdlineDescriptor()
    desc_no_ht.kernel_cmdline = 'root=PARTUUID=$(ANDROID_SYSTEM_PARTUUID)'
    desc_no_ht.flags = (
        AvbKernelCmdlineDescriptor.FLAGS_USE_ONLY_IF_HASHTREE_DISABLED)

    return [desc, desc_no_ht]

  def _get_cmdline_descriptors_for_dm_verity(self, image):
    """Generate kernel cmdline descriptors for dm-verity.

    Arguments:
      image: An ImageHandler (vbmeta or footer) with a hashtree descriptor.

    Returns:
      A list with two AvbKernelCmdlineDescriptor with dm-verity kernel cmdline
      instructions. There is one for when hashtree is not disabled and one for
      when it is.

    Raises:
      AvbError: If  |image| doesn't have a hashtree descriptor.

    """
    (_, _, descriptors, _) = self._parse_image(image)

    ht = None
    for desc in descriptors:
      if isinstance(desc, AvbHashtreeDescriptor):
        ht = desc
        break

    if not ht:
      raise AvbError('No hashtree descriptor in given image')

    return self._get_cmdline_descriptors_for_hashtree_descriptor(ht)

  def make_vbmeta_image(self, output, chain_partitions_use_ab,
                        chain_partitions_do_not_use_ab, algorithm_name,
                        key_path, public_key_metadata_path, rollback_index,
                        flags, rollback_index_location,
                        props, props_from_file, kernel_cmdlines,
                        setup_rootfs_from_kernel,
                        include_descriptors_from_image,
                        signing_helper,
                        signing_helper_with_files,
                        release_string,
                        append_to_release_string,
                        print_required_libavb_version,
                        padding_size):
    """Implements the 'make_vbmeta_image' command.

    Arguments:
      output: File to write the image to.
      chain_partitions_use_ab: List of partitions to chain or None.
      chain_partitions_do_not_use_ab: List of partitions to chain which does not use A/B or None.
      algorithm_name: Name of algorithm to use.
      key_path: Path to key to use or None.
      public_key_metadata_path: Path to public key metadata or None.
      rollback_index: The rollback index to use.
      flags: Flags value to use in the image.
      rollback_index_location: Location of the main vbmeta rollback index.
      props: Properties to insert (list of strings of the form 'key:value').
      props_from_file: Properties to insert (list of strings 'key:<path>').
      kernel_cmdlines: Kernel cmdlines to insert (list of strings).
      setup_rootfs_from_kernel: None or file to generate from.
      include_descriptors_from_image: List of file objects with descriptors.
      signing_helper: Program which signs a hash and return signature.
      signing_helper_with_files: Same as signing_helper but uses files instead.
      release_string: None or avbtool release string to use instead of default.
      append_to_release_string: None or string to append.
      print_required_libavb_version: True to only print required libavb version.
      padding_size: If not 0, pads output so size is a multiple of the number.

    Raises:
      AvbError: If a chained partition is malformed.
    """
    # If we're asked to calculate minimum required libavb version, we're done.
    tmp_header = AvbVBMetaHeader()
    if rollback_index_location > 0:
      tmp_header.bump_required_libavb_version_minor(2)
    if chain_partitions_do_not_use_ab:
      tmp_header.bump_required_libavb_version_minor(3)
    if include_descriptors_from_image:
      # Use the bump logic in AvbVBMetaHeader to calculate the max required
      # version of all included descriptors.
      for image in include_descriptors_from_image:
        (_, image_header, _, _) = self._parse_image(ImageHandler(
            image.name, read_only=True))
        tmp_header.bump_required_libavb_version_minor(
            image_header.required_libavb_version_minor)

    if print_required_libavb_version:
      print('1.{}'.format(tmp_header.required_libavb_version_minor))
      return

    if not output:
      raise AvbError('No output file given')

    descriptors = []
    ht_desc_to_setup = None
    vbmeta_blob = self._generate_vbmeta_blob(
        algorithm_name, key_path, public_key_metadata_path, descriptors,
        chain_partitions_use_ab, chain_partitions_do_not_use_ab,
        rollback_index, flags, rollback_index_location, props, props_from_file,
        kernel_cmdlines, setup_rootfs_from_kernel, ht_desc_to_setup,
        include_descriptors_from_image, signing_helper,
        signing_helper_with_files, release_string,
        append_to_release_string, tmp_header.required_libavb_version_minor)

    # Write entire vbmeta blob (header, authentication, auxiliary).
    output.seek(0)
    output.write(vbmeta_blob)

    if padding_size > 0:
      padded_size = round_to_multiple(len(vbmeta_blob), padding_size)
      padding_needed = padded_size - len(vbmeta_blob)
      output.write(b'\0' * padding_needed)

  def _generate_vbmeta_blob(self, algorithm_name, key_path,
                            public_key_metadata_path, descriptors,
                            chain_partitions_use_ab, chain_partitions_do_not_use_ab,
                            rollback_index, flags, rollback_index_location,
                            props, props_from_file,
                            kernel_cmdlines,
                            setup_rootfs_from_kernel,
                            ht_desc_to_setup,
                            include_descriptors_from_image, signing_helper,
                            signing_helper_with_files,
                            release_string, append_to_release_string,
                            required_libavb_version_minor):
    """Generates a VBMeta blob.

    This blob contains the header (struct AvbVBMetaHeader), the
    authentication data block (which contains the hash and signature
    for the header and auxiliary block), and the auxiliary block
    (which contains descriptors, the public key used, and other data).

    The |key| parameter can |None| only if the |algorithm_name| is
    'NONE'.

    Arguments:
      algorithm_name: The algorithm name as per the ALGORITHMS dict.
      key_path: The path to the .pem file used to sign the blob.
      public_key_metadata_path: Path to public key metadata or None.
      descriptors: A list of descriptors to insert or None.
      chain_partitions_use_ab: List of partitions to chain with A/B or None.
      chain_partitions_do_not_use_ab: List of partitions to chain without A/B or None
      rollback_index: The rollback index to use.
      flags: Flags to use in the image.
      rollback_index_location: Location of the main vbmeta rollback index.
      props: Properties to insert (List of strings of the form 'key:value').
      props_from_file: Properties to insert (List of strings 'key:<path>').
      kernel_cmdlines: Kernel cmdlines to insert (list of strings).
      setup_rootfs_from_kernel: None or file to generate
        dm-verity kernel cmdline from.
      ht_desc_to_setup: If not None, an AvbHashtreeDescriptor to
        generate dm-verity kernel cmdline descriptors from.
      include_descriptors_from_image: List of file objects for which
        to insert descriptors from.
      signing_helper: Program which signs a hash and return signature.
      signing_helper_with_files: Same as signing_helper but uses files instead.
      release_string: None or avbtool release string.
      append_to_release_string: None or string to append.
      required_libavb_version_minor: Use at least this required minor version.

    Returns:
      The VBMeta blob as bytes.

    Raises:
      Exception: If the |algorithm_name| is not found, if no key has
        been given and the given algorithm requires one, or the key is
        of the wrong size.
    """
    try:
      alg = ALGORITHMS[algorithm_name]
    except KeyError as e:
      raise AvbError('Unknown algorithm with name {}'
                     .format(algorithm_name)) from e

    if not descriptors:
      descriptors = []

    h = AvbVBMetaHeader()
    h.bump_required_libavb_version_minor(required_libavb_version_minor)

    # Insert chained partition descriptors, if any
    all_chain_partitions = []
    if chain_partitions_use_ab:
      all_chain_partitions.extend(chain_partitions_use_ab)
    if chain_partitions_do_not_use_ab:
      all_chain_partitions.extend(chain_partitions_do_not_use_ab)

    if len(all_chain_partitions) > 0:
      used_locations = {rollback_index_location: True}
      for cp in all_chain_partitions:
        cp_tokens = cp.split(':')
        if len(cp_tokens) != 3:
          raise AvbError('Malformed chained partition "{}".'.format(cp))
        partition_name = cp_tokens[0]
        chained_rollback_index_location = int(cp_tokens[1])
        file_path = cp_tokens[2]
        # Check that the same rollback location isn't being used by
        # multiple chained partitions.
        if used_locations.get(chained_rollback_index_location):
          raise AvbError('Rollback Index Location {} is already in use.'.format(
              chained_rollback_index_location))
        used_locations[chained_rollback_index_location] = True
        desc = AvbChainPartitionDescriptor()
        desc.partition_name = partition_name
        desc.rollback_index_location = chained_rollback_index_location
        if desc.rollback_index_location < 1:
          raise AvbError('Rollback index location must be 1 or larger.')
        with open(file_path, 'rb') as f:
          desc.public_key = f.read()
        if chain_partitions_do_not_use_ab and (cp in chain_partitions_do_not_use_ab):
          desc.flags |= 1
        descriptors.append(desc)

    # Descriptors.
    encoded_descriptors = bytearray()
    for desc in descriptors:
      encoded_descriptors.extend(desc.encode())

    # Add properties.
    if props:
      for prop in props:
        idx = prop.find(':')
        if idx == -1:
          raise AvbError('Malformed property "{}".'.format(prop))
        # pylint: disable=redefined-variable-type
        desc = AvbPropertyDescriptor()
        desc.key = prop[0:idx]
        desc.value = prop[(idx + 1):].encode('utf-8')
        encoded_descriptors.extend(desc.encode())
    if props_from_file:
      for prop in props_from_file:
        idx = prop.find(':')
        if idx == -1:
          raise AvbError('Malformed property "{}".'.format(prop))
        desc = AvbPropertyDescriptor()
        desc.key = prop[0:idx]
        file_path = prop[(idx + 1):]
        with open(file_path, 'rb') as f:
          # pylint: disable=attribute-defined-outside-init
          desc.value = f.read()
        encoded_descriptors.extend(desc.encode())

    # Add AvbKernelCmdline descriptor for dm-verity from an image, if requested.
    if setup_rootfs_from_kernel:
      image_handler = ImageHandler(
          setup_rootfs_from_kernel.name)
      cmdline_desc = self._get_cmdline_descriptors_for_dm_verity(image_handler)
      encoded_descriptors.extend(cmdline_desc[0].encode())
      encoded_descriptors.extend(cmdline_desc[1].encode())

    # Add AvbKernelCmdline descriptor for dm-verity from desc, if requested.
    if ht_desc_to_setup:
      cmdline_desc = self._get_cmdline_descriptors_for_hashtree_descriptor(
          ht_desc_to_setup)
      encoded_descriptors.extend(cmdline_desc[0].encode())
      encoded_descriptors.extend(cmdline_desc[1].encode())

    # Add kernel command-lines.
    if kernel_cmdlines:
      for i in kernel_cmdlines:
        desc = AvbKernelCmdlineDescriptor()
        desc.kernel_cmdline = i
        encoded_descriptors.extend(desc.encode())

    # Add descriptors from other images.
    if include_descriptors_from_image:
      descriptors_dict = dict()
      for image in include_descriptors_from_image:
        image_handler = ImageHandler(image.name, read_only=True)
        (_, image_vbmeta_header, image_descriptors, _) = self._parse_image(
            image_handler)
        # Bump the required libavb version to support all included descriptors.
        h.bump_required_libavb_version_minor(
            image_vbmeta_header.required_libavb_version_minor)
        for desc in image_descriptors:
          # The --include_descriptors_from_image option is used in some setups
          # with images A and B where both A and B contain a descriptor
          # for a partition with the same name. Since it's not meaningful
          # to include both descriptors, only include the last seen descriptor.
          # See bug 76386656 for details.
          if hasattr(desc, 'partition_name'):
            key = type(desc).__name__ + '_' + desc.partition_name
            descriptors_dict[key] = desc.encode()
          else:
            encoded_descriptors.extend(desc.encode())
      for key in sorted(descriptors_dict):
        encoded_descriptors.extend(descriptors_dict[key])

    # Load public key metadata blob, if requested.
    pkmd_blob = b''
    if public_key_metadata_path:
      with open(public_key_metadata_path, 'rb') as f:
        pkmd_blob = f.read()

    key = None
    encoded_key = b''
    if alg.public_key_num_bytes > 0:
      if not key_path:
        raise AvbError('Key is required for algorithm {}'.format(
            algorithm_name))
      encoded_key = RSAPublicKey(key_path).encode()
      if len(encoded_key) != alg.public_key_num_bytes:
        raise AvbError('Key is wrong size for algorithm {}'.format(
            algorithm_name))

    # Override release string, if requested.
    if isinstance(release_string, str):
      h.release_string = release_string

    # Append to release string, if requested. Also insert a space before.
    if isinstance(append_to_release_string, str):
      h.release_string += ' ' + append_to_release_string

    # For the Auxiliary data block, descriptors are stored at offset 0,
    # followed by the public key, followed by the public key metadata blob.
    h.auxiliary_data_block_size = round_to_multiple(
        len(encoded_descriptors) + len(encoded_key) + len(pkmd_blob), 64)
    h.descriptors_offset = 0
    h.descriptors_size = len(encoded_descriptors)
    h.public_key_offset = h.descriptors_size
    h.public_key_size = len(encoded_key)
    h.public_key_metadata_offset = h.public_key_offset + h.public_key_size
    h.public_key_metadata_size = len(pkmd_blob)

    # For the Authentication data block, the hash is first and then
    # the signature.
    h.authentication_data_block_size = round_to_multiple(
        alg.hash_num_bytes + alg.signature_num_bytes, 64)
    h.algorithm_type = alg.algorithm_type
    h.hash_offset = 0
    h.hash_size = alg.hash_num_bytes
    # Signature offset and size - it's stored right after the hash
    # (in Authentication data block).
    h.signature_offset = alg.hash_num_bytes
    h.signature_size = alg.signature_num_bytes

    h.rollback_index = rollback_index
    h.flags = flags
    h.rollback_index_location = rollback_index_location

    # Generate Header data block.
    header_data_blob = h.encode()

    # Generate Auxiliary data block.
    aux_data_blob = bytearray()
    aux_data_blob.extend(encoded_descriptors)
    aux_data_blob.extend(encoded_key)
    aux_data_blob.extend(pkmd_blob)
    padding_bytes = h.auxiliary_data_block_size - len(aux_data_blob)
    aux_data_blob.extend(b'\0' * padding_bytes)

    # Calculate the hash.
    binary_hash = b''
    binary_signature = b''
    if algorithm_name != 'NONE':
      ha = hashlib.new(alg.hash_name)
      ha.update(header_data_blob)
      ha.update(aux_data_blob)
      binary_hash = ha.digest()

      # Calculate the signature.
      rsa_key = RSAPublicKey(key_path)
      data_to_sign = header_data_blob + bytes(aux_data_blob)
      binary_signature = rsa_key.sign(algorithm_name, data_to_sign,
                                      signing_helper, signing_helper_with_files)

    # Generate Authentication data block.
    auth_data_blob = bytearray()
    auth_data_blob.extend(binary_hash)
    auth_data_blob.extend(binary_signature)
    padding_bytes = h.authentication_data_block_size - len(auth_data_blob)
    auth_data_blob.extend(b'\0' * padding_bytes)

    return header_data_blob + bytes(auth_data_blob) + bytes(aux_data_blob)

  def extract_public_key(self, key_path, output):
    """Implements the 'extract_public_key' command.

    Arguments:
      key_path: The path to a RSA private key file.
      output: The file to write to.

    Raises:
      AvbError: If the public key could not be extracted.
    """
    output.write(RSAPublicKey(key_path).encode())

  def extract_public_key_digest(self, key_path, output):
    """Implements the 'extract_public_key_digest' command.

    Arguments:
      key_path: The path to a RSA private key file.
      output: The file to write to.

    Raises:
      AvbError: If the public key could not be extracted.
    """
    hasher = hashlib.sha256()
    hasher.update(RSAPublicKey(key_path).encode())
    output.write(hasher.hexdigest())

  def append_vbmeta_image(self, image_filename, vbmeta_image_filename,
                          partition_size):
    """Implementation of the append_vbmeta_image command.

    Arguments:
      image_filename: File to add the footer to.
      vbmeta_image_filename: File to get vbmeta struct from.
      partition_size: Size of partition.

    Raises:
      AvbError: If an argument is incorrect or if appending VBMeta image fialed.
    """
    image = ImageHandler(image_filename)

    if partition_size % image.block_size != 0:
      raise AvbError('Partition size of {} is not a multiple of the image '
                     'block size {}.'.format(partition_size,
                                             image.block_size))

    # If there's already a footer, truncate the image to its original
    # size. This way 'avbtool append_vbmeta_image' is idempotent.
    if image.image_size >= AvbFooter.SIZE:
      image.seek(image.image_size - AvbFooter.SIZE)
      try:
        footer = AvbFooter(image.read(AvbFooter.SIZE))
        # Existing footer found. Just truncate.
        original_image_size = footer.original_image_size
        image.truncate(footer.original_image_size)
      except (LookupError, struct.error):
        original_image_size = image.image_size
    else:
      # Image size is too small to possibly contain a footer.
      original_image_size = image.image_size

    # If anything goes wrong from here-on, restore the image back to
    # its original size.
    try:
      vbmeta_image_handler = ImageHandler(vbmeta_image_filename)
      vbmeta_blob = self._load_vbmeta_blob(vbmeta_image_handler)

      # If the image isn't sparse, its size might not be a multiple of
      # the block size. This will screw up padding later so just grow it.
      if image.image_size % image.block_size != 0:
        assert not image.is_sparse
        padding_needed = image.block_size - (image.image_size%image.block_size)
        image.truncate(image.image_size + padding_needed)

      # The append_raw() method requires content with size being a
      # multiple of |block_size| so add padding as needed. Also record
      # where this is written to since we'll need to put that in the
      # footer.
      vbmeta_offset = image.image_size
      padding_needed = (round_to_multiple(len(vbmeta_blob), image.block_size) -
                        len(vbmeta_blob))
      vbmeta_blob_with_padding = vbmeta_blob + b'\0' * padding_needed

      # Append vbmeta blob and footer
      image.append_raw(vbmeta_blob_with_padding)
      vbmeta_end_offset = vbmeta_offset + len(vbmeta_blob_with_padding)

      # Now insert a DONT_CARE chunk with enough bytes such that the
      # final Footer block is at the end of partition_size..
      image.append_dont_care(partition_size - vbmeta_end_offset -
                             1 * image.block_size)

      # Generate the Footer that tells where the VBMeta footer
      # is. Also put enough padding in the front of the footer since
      # we'll write out an entire block.
      footer = AvbFooter()
      footer.original_image_size = original_image_size
      footer.vbmeta_offset = vbmeta_offset
      footer.vbmeta_size = len(vbmeta_blob)
      footer_blob = footer.encode()
      footer_blob_with_padding = (b'\0' * (image.block_size - AvbFooter.SIZE) +
                                  footer_blob)
      image.append_raw(footer_blob_with_padding)

    except Exception as e:
      # Truncate back to original size, then re-raise.
      image.truncate(original_image_size)
      raise AvbError('Appending VBMeta image failed: {}.'.format(e)) from e

  def add_hash_footer(self, image_filename, partition_size,
                      dynamic_partition_size, partition_name,
                      hash_algorithm, salt, chain_partitions_use_ab,
                      chain_partitions_do_not_use_ab,
                      algorithm_name, key_path,
                      public_key_metadata_path, rollback_index, flags,
                      rollback_index_location, props,
                      props_from_file, kernel_cmdlines,
                      setup_rootfs_from_kernel,
                      include_descriptors_from_image, calc_max_image_size,
                      signing_helper, signing_helper_with_files,
                      release_string, append_to_release_string,
                      output_vbmeta_image, do_not_append_vbmeta_image,
                      print_required_libavb_version, use_persistent_digest,
                      do_not_use_ab):
    """Implementation of the add_hash_footer on unsparse images.

    Arguments:
      image_filename: File to add the footer to.
      partition_size: Size of partition.
      dynamic_partition_size: Calculate partition size based on image size.
      partition_name: Name of partition (without A/B suffix).
      hash_algorithm: Hash algorithm to use.
      salt: Salt to use as a hexadecimal string or None to use /dev/urandom.
      chain_partitions_use_ab: List of partitions to chain with A/B or None.
      chain_partitions_do_not_use_ab: List of partitions to chain without A/B or None.
      algorithm_name: Name of algorithm to use.
      key_path: Path to key to use or None.
      public_key_metadata_path: Path to public key metadata or None.
      rollback_index: Rollback index.
      flags: Flags value to use in the image.
      rollback_index_location: Location of the main vbmeta rollback index.
      props: Properties to insert (List of strings of the form 'key:value').
      props_from_file: Properties to insert (List of strings 'key:<path>').
      kernel_cmdlines: Kernel cmdlines to insert (list of strings).
      setup_rootfs_from_kernel: None or file to generate
        dm-verity kernel cmdline from.
      include_descriptors_from_image: List of file objects for which
        to insert descriptors from.
      calc_max_image_size: Don't store the footer - instead calculate the
        maximum image size leaving enough room for metadata with the
        given |partition_size|.
      signing_helper: Program which signs a hash and return signature.
      signing_helper_with_files: Same as signing_helper but uses files instead.
      release_string: None or avbtool release string.
      append_to_release_string: None or string to append.
      output_vbmeta_image: If not None, also write vbmeta struct to this file.
      do_not_append_vbmeta_image: If True, don't append vbmeta struct.
      print_required_libavb_version: True to only print required libavb version.
      use_persistent_digest: Use a persistent digest on device.
      do_not_use_ab: This partition does not use A/B.

    Raises:
      AvbError: If an argument is incorrect of if adding of hash_footer failed.
    """
    if not partition_size and not dynamic_partition_size:
      raise AvbError('--dynamic_partition_size required when not specifying a '
                     'partition size')

    if dynamic_partition_size and calc_max_image_size:
      raise AvbError('--calc_max_image_size not supported with '
                     '--dynamic_partition_size')

    required_libavb_version_minor = 0
    if use_persistent_digest or do_not_use_ab:
      required_libavb_version_minor = 1
    if rollback_index_location > 0:
      required_libavb_version_minor = 2
    if chain_partitions_do_not_use_ab:
      required_libavb_version_minor = 3

    # If we're asked to calculate minimum required libavb version, we're done.
    if print_required_libavb_version:
      print('1.{}'.format(required_libavb_version_minor))
      return

    # First, calculate the maximum image size such that an image
    # this size + metadata (footer + vbmeta struct) fits in
    # |partition_size|.
    max_metadata_size = self.MAX_VBMETA_SIZE + self.MAX_FOOTER_SIZE
    if not dynamic_partition_size and partition_size < max_metadata_size:
      raise AvbError('Parition size of {} is too small. '
                     'Needs to be at least {}'.format(
                         partition_size, max_metadata_size))

    # If we're asked to only calculate the maximum image size, we're done.
    if calc_max_image_size:
      print('{}'.format(partition_size - max_metadata_size))
      return

    # If we aren't appending the vbmeta footer to the input image we can
    # open it in read-only mode.
    image = ImageHandler(image_filename,
                         read_only=do_not_append_vbmeta_image)

    # If there's already a footer, truncate the image to its original
    # size. This way 'avbtool add_hash_footer' is idempotent (modulo
    # salts).
    if image.image_size >= AvbFooter.SIZE:
      image.seek(image.image_size - AvbFooter.SIZE)
      try:
        footer = AvbFooter(image.read(AvbFooter.SIZE))
        # Existing footer found. Just truncate.
        original_image_size = footer.original_image_size
        image.truncate(footer.original_image_size)
      except (LookupError, struct.error):
        original_image_size = image.image_size
    else:
      # Image size is too small to possibly contain a footer.
      original_image_size = image.image_size

    if dynamic_partition_size:
      partition_size = round_to_multiple(
          original_image_size + max_metadata_size, image.block_size)

    max_image_size = partition_size - max_metadata_size
    if partition_size % image.block_size != 0:
      raise AvbError('Partition size of {} is not a multiple of the image '
                     'block size {}.'.format(partition_size,
                                             image.block_size))

    # If anything goes wrong from here-on, restore the image back to
    # its original size.
    try:
      # If image size exceeds the maximum image size, fail.
      if image.image_size > max_image_size:
        raise AvbError('Image size of {} exceeds maximum image '
                       'size of {} in order to fit in a partition '
                       'size of {}.'.format(image.image_size, max_image_size,
                                            partition_size))

      digest_size = len(hashlib.new(hash_algorithm).digest())
      if salt:
        salt = binascii.unhexlify(salt)
      elif salt is None and not use_persistent_digest:
        # If salt is not explicitly specified, choose a hash that's the same
        # size as the hash size. Don't populate a random salt if this
        # descriptor is being created to use a persistent digest on device.
        hash_size = digest_size
        with open('/dev/urandom', 'rb') as f:
          salt = f.read(hash_size)
      else:
        salt = b''

      hasher = hashlib.new(hash_algorithm, salt)
      # TODO(zeuthen): might want to read this in chunks to avoid
      # memory pressure, then again, this is only supposed to be used
      # on kernel/initramfs partitions. Possible optimization.
      image.seek(0)
      hasher.update(image.read(image.image_size))
      digest = hasher.digest()

      h_desc = AvbHashDescriptor()
      h_desc.image_size = image.image_size
      h_desc.hash_algorithm = hash_algorithm
      h_desc.partition_name = partition_name
      h_desc.salt = salt
      h_desc.flags = 0
      if do_not_use_ab:
        h_desc.flags |= 1  # AVB_HASH_DESCRIPTOR_FLAGS_DO_NOT_USE_AB
      if not use_persistent_digest:
        h_desc.digest = digest

      # Generate the VBMeta footer.
      ht_desc_to_setup = None
      vbmeta_blob = self._generate_vbmeta_blob(
          algorithm_name, key_path, public_key_metadata_path, [h_desc],
          chain_partitions_use_ab, chain_partitions_do_not_use_ab, rollback_index,
          flags, rollback_index_location, props, props_from_file,
          kernel_cmdlines, setup_rootfs_from_kernel, ht_desc_to_setup,
          include_descriptors_from_image, signing_helper,
          signing_helper_with_files, release_string,
          append_to_release_string, required_libavb_version_minor)

      # Write vbmeta blob, if requested.
      if output_vbmeta_image:
        output_vbmeta_image.write(vbmeta_blob)

      # Append vbmeta blob and footer, unless requested not to.
      if not do_not_append_vbmeta_image:
        # If the image isn't sparse, its size might not be a multiple of
        # the block size. This will screw up padding later so just grow it.
        if image.image_size % image.block_size != 0:
          assert not image.is_sparse
          padding_needed = image.block_size - (
              image.image_size % image.block_size)
          image.truncate(image.image_size + padding_needed)

        # The append_raw() method requires content with size being a
        # multiple of |block_size| so add padding as needed. Also record
        # where this is written to since we'll need to put that in the
        # footer.
        vbmeta_offset = image.image_size
        padding_needed = (
            round_to_multiple(len(vbmeta_blob), image.block_size) -
            len(vbmeta_blob))
        vbmeta_blob_with_padding = vbmeta_blob + b'\0' * padding_needed

        image.append_raw(vbmeta_blob_with_padding)
        vbmeta_end_offset = vbmeta_offset + len(vbmeta_blob_with_padding)

        # Now insert a DONT_CARE chunk with enough bytes such that the
        # final Footer block is at the end of partition_size..
        image.append_dont_care(partition_size - vbmeta_end_offset -
                               1 * image.block_size)

        # Generate the Footer that tells where the VBMeta footer
        # is. Also put enough padding in the front of the footer since
        # we'll write out an entire block.
        footer = AvbFooter()
        footer.original_image_size = original_image_size
        footer.vbmeta_offset = vbmeta_offset
        footer.vbmeta_size = len(vbmeta_blob)
        footer_blob = footer.encode()
        footer_blob_with_padding = (
            b'\0' * (image.block_size - AvbFooter.SIZE) + footer_blob)
        image.append_raw(footer_blob_with_padding)
    except Exception as e:
      # Truncate back to original size, then re-raise.
      image.truncate(original_image_size)
      raise AvbError('Adding hash_footer failed: {}.'.format(e)) from e

  def add_hashtree_footer(self, image_filename, partition_size: int, partition_name,
                          generate_fec, fec_num_roots, hash_algorithm,
                          block_size, salt, chain_partitions_use_ab,
                          chain_partitions_do_not_use_ab,
                          algorithm_name, key_path,
                          public_key_metadata_path, rollback_index, flags,
                          rollback_index_location: int,
                          props, props_from_file, kernel_cmdlines,
                          setup_rootfs_from_kernel,
                          setup_as_rootfs_from_kernel,
                          include_descriptors_from_image,
                          calc_max_image_size, signing_helper,
                          signing_helper_with_files,
                          release_string, append_to_release_string,
                          output_vbmeta_image, do_not_append_vbmeta_image,
                          print_required_libavb_version,
                          use_persistent_root_digest, do_not_use_ab,
                          no_hashtree, check_at_most_once):
    """Implements the 'add_hashtree_footer' command.

    See https://gitlab.com/cryptsetup/cryptsetup/wikis/DMVerity for
    more information about dm-verity and these hashes.

    Arguments:
      image_filename: File to add the footer to.
      partition_size: Size of partition or 0 to put it right at the end.
      partition_name: Name of partition (without A/B suffix).
      generate_fec: If True, generate FEC codes.
      fec_num_roots: Number of roots for FEC.
      hash_algorithm: Hash algorithm to use.
      block_size: Block size to use.
      salt: Salt to use as a hexadecimal string or None to use /dev/urandom.
      chain_partitions_use_ab: List of partitions to chain.
      chain_partitions_do_not_use_ab: List of partitions to chain without A/B or None.
      algorithm_name: Name of algorithm to use.
      key_path: Path to key to use or None.
      public_key_metadata_path: Path to public key metadata or None.
      rollback_index: Rollback index.
      flags: Flags value to use in the image.
      rollback_index_location: Location of the main vbmeta rollback index.
      props: Properties to insert (List of strings of the form 'key:value').
      props_from_file: Properties to insert (List of strings 'key:<path>').
      kernel_cmdlines: Kernel cmdlines to insert (list of strings).
      setup_rootfs_from_kernel: None or file to generate
        dm-verity kernel cmdline from.
      setup_as_rootfs_from_kernel: If True, generate dm-verity kernel
        cmdline to set up rootfs.
      include_descriptors_from_image: List of file objects for which
        to insert descriptors from.
      calc_max_image_size: Don't store the hashtree or footer - instead
        calculate the maximum image size leaving enough room for hashtree
        and metadata with the given |partition_size|.
      signing_helper: Program which signs a hash and return signature.
      signing_helper_with_files: Same as signing_helper but uses files instead.
      release_string: None or avbtool release string.
      append_to_release_string: None or string to append.
      output_vbmeta_image: If not None, also write vbmeta struct to this file.
      do_not_append_vbmeta_image: If True, don't append vbmeta struct.
      print_required_libavb_version: True to only print required libavb version.
      use_persistent_root_digest: Use a persistent root digest on device.
      do_not_use_ab: The partition does not use A/B.
      no_hashtree: Do not append hashtree. Set size in descriptor as zero.
      check_at_most_once: Set to verify data blocks only the first time they
        are read from the data device.

    Raises:
      AvbError: If an argument is incorrect or adding the hashtree footer
          failed.
    """
    required_libavb_version_minor = 0
    if use_persistent_root_digest or do_not_use_ab or check_at_most_once:
      required_libavb_version_minor = 1
    if rollback_index_location > 0:
      required_libavb_version_minor = 2
    if chain_partitions_do_not_use_ab:
      required_libavb_version_minor = 3

    # If we're asked to calculate minimum required libavb version, we're done.
    if print_required_libavb_version:
      print('1.{}'.format(required_libavb_version_minor))
      return

    digest_size = len(create_avb_hashtree_hasher(hash_algorithm, b'')
                      .digest())
    digest_padding = round_to_pow2(digest_size) - digest_size

    # If |partition_size| is given (e.g. not 0), calculate the maximum image
    # size such that an image this size + the hashtree + metadata (footer +
    # vbmeta struct) fits in |partition_size|. We use very conservative figures
    # for metadata.
    if partition_size > 0:
      max_tree_size = 0
      max_fec_size = 0
      if not no_hashtree:
        (_, max_tree_size) = calc_hash_level_offsets(
            partition_size, block_size, digest_size + digest_padding)
        if generate_fec:
          max_fec_size = calc_fec_data_size(partition_size, fec_num_roots)
      max_metadata_size = (max_fec_size + max_tree_size +
                           self.MAX_VBMETA_SIZE +
                           self.MAX_FOOTER_SIZE)
      max_image_size = partition_size - max_metadata_size
    else:
      max_image_size = 0

    # If we're asked to only calculate the maximum image size, we're done.
    if calc_max_image_size:
      print('{}'.format(max_image_size))
      return

    image = ImageHandler(image_filename)

    if partition_size > 0:
      if partition_size % image.block_size != 0:
        raise AvbError('Partition size of {} is not a multiple of the image '
                       'block size {}.'.format(partition_size,
                                               image.block_size))
    elif image.image_size % image.block_size != 0:
      raise AvbError('File size of {} is not a multiple of the image '
                     'block size {}.'.format(image.image_size,
                                             image.block_size))

    # If there's already a footer, truncate the image to its original
    # size. This way 'avbtool add_hashtree_footer' is idempotent
    # (modulo salts).
    if image.image_size >= AvbFooter.SIZE:
      image.seek(image.image_size - AvbFooter.SIZE)
      try:
        footer = AvbFooter(image.read(AvbFooter.SIZE))
        # Existing footer found. Just truncate.
        original_image_size = footer.original_image_size
        image.truncate(footer.original_image_size)
      except (LookupError, struct.error):
        original_image_size = image.image_size
    else:
      # Image size is too small to possibly contain a footer.
      original_image_size = image.image_size

    # If anything goes wrong from here-on, restore the image back to
    # its original size.
    try:
      # Ensure image is multiple of block_size.
      rounded_image_size = round_to_multiple(image.image_size, block_size)
      if rounded_image_size > image.image_size:
        # If we need to round up the image size, it means the length of the
        # data to append is not a multiple of block size.
        # Setting multiple_block_size to false, so append_raw() will not
        # require it.
        image.append_raw(b'\0' * (rounded_image_size - image.image_size),
                         multiple_block_size=False)

      # If image size exceeds the maximum image size, fail.
      if partition_size > 0:
        if image.image_size > max_image_size:
          raise AvbError('Image size of {} exceeds maximum image '
                         'size of {} in order to fit in a partition '
                         'size of {}.'.format(image.image_size, max_image_size,
                                              partition_size))

      if salt:
        salt = binascii.unhexlify(salt)
      elif salt is None and not use_persistent_root_digest:
        # If salt is not explicitly specified, choose a hash that's the same
        # size as the hash size. Don't populate a random salt if this
        # descriptor is being created to use a persistent digest on device.
        hash_size = digest_size
        with open('/dev/urandom', 'rb') as f:
          salt = f.read(hash_size)
      else:
        salt = b''

      # Hashes are stored upside down so we need to calculate hash
      # offsets in advance.
      (hash_level_offsets, tree_size) = calc_hash_level_offsets(
          image.image_size, block_size, digest_size + digest_padding)

      # If the image isn't sparse, its size might not be a multiple of
      # the block size. This will screw up padding later so just grow it.
      if image.image_size % image.block_size != 0:
        assert not image.is_sparse
        padding_needed = image.block_size - (image.image_size%image.block_size)
        image.truncate(image.image_size + padding_needed)

      # Generate the tree and add padding as needed.
      tree_offset = image.image_size
      root_digest, hash_tree = generate_hash_tree(image, image.image_size,
                                                  block_size,
                                                  hash_algorithm, salt,
                                                  digest_padding,
                                                  hash_level_offsets,
                                                  tree_size)

      # Generate HashtreeDescriptor with details about the tree we
      # just generated.
      if no_hashtree:
        tree_size = 0
        hash_tree = b''
      ht_desc = AvbHashtreeDescriptor()
      ht_desc.dm_verity_version = 1
      ht_desc.image_size = image.image_size
      ht_desc.tree_offset = tree_offset
      ht_desc.tree_size = tree_size
      ht_desc.data_block_size = block_size
      ht_desc.hash_block_size = block_size
      ht_desc.hash_algorithm = hash_algorithm
      ht_desc.partition_name = partition_name
      ht_desc.salt = salt
      if do_not_use_ab:
        ht_desc.flags |= AvbHashtreeDescriptor.FLAGS_DO_NOT_USE_AB
      if not use_persistent_root_digest:
        ht_desc.root_digest = root_digest
      if check_at_most_once:
        ht_desc.flags |= AvbHashtreeDescriptor.FLAGS_CHECK_AT_MOST_ONCE

      # Write the hash tree
      padding_needed = (round_to_multiple(len(hash_tree), image.block_size) -
                        len(hash_tree))
      hash_tree_with_padding = hash_tree + b'\0' * padding_needed
      if len(hash_tree_with_padding) > 0:
        image.append_raw(hash_tree_with_padding)
      len_hashtree_and_fec = len(hash_tree_with_padding)

      # Generate FEC codes, if requested.
      if generate_fec:
        if no_hashtree:
          fec_data = b''
        else:
          fec_data = generate_fec_data(image_filename, fec_num_roots)
        padding_needed = (round_to_multiple(len(fec_data), image.block_size) -
                          len(fec_data))
        fec_data_with_padding = fec_data + b'\0' * padding_needed
        fec_offset = image.image_size
        image.append_raw(fec_data_with_padding)
        len_hashtree_and_fec += len(fec_data_with_padding)
        # Update the hashtree descriptor.
        ht_desc.fec_num_roots = fec_num_roots
        ht_desc.fec_offset = fec_offset
        ht_desc.fec_size = len(fec_data)

      ht_desc_to_setup = None
      if setup_as_rootfs_from_kernel:
        ht_desc_to_setup = ht_desc

      # Generate the VBMeta footer and add padding as needed.
      vbmeta_offset = tree_offset + len_hashtree_and_fec
      vbmeta_blob = self._generate_vbmeta_blob(
          algorithm_name, key_path, public_key_metadata_path, [ht_desc],
          chain_partitions_use_ab, chain_partitions_do_not_use_ab,
          rollback_index, flags, rollback_index_location,
          props, props_from_file,
          kernel_cmdlines, setup_rootfs_from_kernel, ht_desc_to_setup,
          include_descriptors_from_image, signing_helper,
          signing_helper_with_files, release_string,
          append_to_release_string, required_libavb_version_minor)
      padding_needed = (round_to_multiple(len(vbmeta_blob), image.block_size) -
                        len(vbmeta_blob))
      vbmeta_blob_with_padding = vbmeta_blob + b'\0' * padding_needed

      # Write vbmeta blob, if requested.
      if output_vbmeta_image:
        output_vbmeta_image.write(vbmeta_blob)

      # Append vbmeta blob and footer, unless requested not to.
      if not do_not_append_vbmeta_image:
        image.append_raw(vbmeta_blob_with_padding)

        # Now insert a DONT_CARE chunk with enough bytes such that the
        # final Footer block is at the end of partition_size..
        if partition_size > 0:
          image.append_dont_care(partition_size - image.image_size -
                                 1 * image.block_size)

        # Generate the Footer that tells where the VBMeta footer
        # is. Also put enough padding in the front of the footer since
        # we'll write out an entire block.
        footer = AvbFooter()
        footer.original_image_size = original_image_size
        footer.vbmeta_offset = vbmeta_offset
        footer.vbmeta_size = len(vbmeta_blob)
        footer_blob = footer.encode()
        footer_blob_with_padding = (
            b'\0' * (image.block_size - AvbFooter.SIZE) + footer_blob)
        image.append_raw(footer_blob_with_padding)

    except Exception as e:
      # Truncate back to original size, then re-raise.
      image.truncate(original_image_size)
      raise AvbError('Adding hashtree_footer failed: {}.'.format(e)) from e

  def make_certificate(self, output, authority_key_path, subject_key_path,
                       subject_key_version, subject, usage,
                       signing_helper, signing_helper_with_files):
    """Implements the 'make_certificate' command.

    Certificates are required for avb_cert extension public key metadata. They
    chain the vbmeta signing key for a particular product back to a fused,
    permanent root key. These certificates are fixed-length and fixed-format
    with the explicit goal of not parsing ASN.1 in bootloader code.

    Arguments:
      output: Certificate will be written to this file on success.
      authority_key_path: A PEM file path with the authority private key.
                          If None, then a certificate will be created without a
                          signature. The signature can be created out-of-band
                          and appended.
      subject_key_path: Path to a PEM or DER subject public key.
      subject_key_version: A 64-bit version value. If this is None, the number
                           of seconds since the epoch is used.
      subject: A subject identifier. For Product Signing Key certificates this
               should be the same Product ID found in the permanent attributes.
      usage: Usage string whose SHA256 hash will be embedded in the certificate.
      signing_helper: Program which signs a hash and returns the signature.
      signing_helper_with_files: Same as signing_helper but uses files instead.

    Raises:
      AvbError: If there an error during signing.
    """
    signed_data = bytearray()
    signed_data.extend(struct.pack('<I', 1))  # Format Version
    signed_data.extend(RSAPublicKey(subject_key_path).encode())
    hasher = hashlib.sha256()
    hasher.update(subject)
    signed_data.extend(hasher.digest())
    hasher = hashlib.sha256()
    hasher.update(usage.encode('ascii'))
    signed_data.extend(hasher.digest())
    if subject_key_version is None:
      subject_key_version = int(time.time())
    signed_data.extend(struct.pack('<Q', subject_key_version))
    signature = b''
    if authority_key_path:
      rsa_key = RSAPublicKey(authority_key_path)
      algorithm_name = 'SHA512_RSA4096'
      signature = rsa_key.sign(algorithm_name, signed_data, signing_helper,
                               signing_helper_with_files)
    output.write(signed_data)
    output.write(signature)

  def make_cert_permanent_attributes(self, output, root_authority_key_path,
                                     product_id):
    """Implements the 'make_cert_permanent_attributes' command.

    avb_cert permanent attributes are designed to be permanent for a
    particular product and a hash of these attributes should be fused into
    hardware to enforce this.

    Arguments:
      output: Attributes will be written to this file on success.
      root_authority_key_path: Path to a PEM or DER public key for
        the root authority.
      product_id: A 16-byte Product ID.

    Raises:
      AvbError: If an argument is incorrect.
    """
    EXPECTED_PRODUCT_ID_SIZE = 16  # pylint: disable=invalid-name
    if len(product_id) != EXPECTED_PRODUCT_ID_SIZE:
      raise AvbError('Invalid Product ID length.')
    output.write(struct.pack('<I', 1))  # Format Version
    output.write(RSAPublicKey(root_authority_key_path).encode())
    output.write(product_id)

  def make_cert_metadata(self, output, intermediate_key_certificate,
                         product_key_certificate):
    """Implements the 'make_cert_metadata' command.

    avb_cert metadata are included in vbmeta images to facilitate
    verification. The output of this command can be used as the
    public_key_metadata argument to other commands.

    Arguments:
      output: Metadata will be written to this file on success.
      intermediate_key_certificate: A certificate file as output by
                                    make_certificate with usage set to
                                    CERT_USAGE_INTERMEDIATE_AUTHORITY.
      product_key_certificate: A certificate file as output by
                               make_certificate with usage set to
                               CERT_USAGE_SIGNING.

    Raises:
      AvbError: If an argument is incorrect.
    """
    EXPECTED_CERTIFICATE_SIZE = 1620  # pylint: disable=invalid-name
    if len(intermediate_key_certificate) != EXPECTED_CERTIFICATE_SIZE:
      raise AvbError('Invalid intermediate key certificate length.')
    if len(product_key_certificate) != EXPECTED_CERTIFICATE_SIZE:
      raise AvbError('Invalid product key certificate length.')
    output.write(struct.pack('<I', 1))  # Format Version
    output.write(intermediate_key_certificate)
    output.write(product_key_certificate)

  def make_cert_unlock_credential(self, output, intermediate_key_certificate,
                                  unlock_key_certificate, challenge_path,
                                  unlock_key_path, signing_helper,
                                  signing_helper_with_files):
    """Implements the 'make_cert_unlock_credential' command.

    avb_cert unlock credentials can be used to authorize the unlock of AVB
    on a device. These credentials are presented to an avb_cert bootloader
    via the fastboot interface in response to a 16-byte challenge. This method
    creates all fields of the credential except the challenge signature field
    (which is the last field) and can optionally create the challenge signature
    field as well if a challenge and the unlock_key_path is provided.

    Arguments:
      output: The credential will be written to this file on success.
      intermediate_key_certificate: A certificate file as output by
                                    make_certificate with usage set to
                                    CERT_USAGE_INTERMEDIATE_AUTHORITY.
      unlock_key_certificate: A certificate file as output by
                              make_certificate with usage set to
                              CERT_USAGE_UNLOCK.
      challenge_path: [optional] A path to the challenge to sign.
      unlock_key_path: [optional] A PEM file path with the unlock private key.
      signing_helper: Program which signs a hash and returns the signature.
      signing_helper_with_files: Same as signing_helper but uses files instead.

    Raises:
      AvbError: If an argument is incorrect or an error occurs during signing.
    """
    EXPECTED_CERTIFICATE_SIZE = 1620  # pylint: disable=invalid-name
    EXPECTED_CHALLENGE_SIZE = 16  # pylint: disable=invalid-name
    if len(intermediate_key_certificate) != EXPECTED_CERTIFICATE_SIZE:
      raise AvbError('Invalid intermediate key certificate length.')
    if len(unlock_key_certificate) != EXPECTED_CERTIFICATE_SIZE:
      raise AvbError('Invalid product key certificate length.')
    challenge = b''
    if challenge_path:
      with open(challenge_path, 'rb') as f:
        challenge = f.read()
      if len(challenge) != EXPECTED_CHALLENGE_SIZE:
        raise AvbError('Invalid unlock challenge length.')
    output.write(struct.pack('<I', 1))  # Format Version
    output.write(intermediate_key_certificate)
    output.write(unlock_key_certificate)
    if challenge_path and unlock_key_path:
      rsa_key = RSAPublicKey(unlock_key_path)
      algorithm_name = 'SHA512_RSA4096'
      signature = rsa_key.sign(algorithm_name, challenge, signing_helper,
                               signing_helper_with_files)
      output.write(signature)


def calc_hash_level_offsets(image_size, block_size, digest_size):
  """Calculate the offsets of all the hash-levels in a Merkle-tree.

  Arguments:
    image_size: The size of the image to calculate a Merkle-tree for.
    block_size: The block size, e.g. 4096.
    digest_size: The size of each hash, e.g. 32 for SHA-256.

  Returns:
    A tuple where the first argument is an array of offsets and the
    second is size of the tree, in bytes.
  """
  level_offsets = []
  level_sizes = []
  tree_size = 0

  num_levels = 0
  size = image_size
  while size > block_size:
    num_blocks = (size + block_size - 1) // block_size
    level_size = round_to_multiple(num_blocks * digest_size, block_size)

    level_sizes.append(level_size)
    tree_size += level_size
    num_levels += 1

    size = level_size

  for n in range(0, num_levels):
    offset = 0
    for m in range(n + 1, num_levels):
      offset += level_sizes[m]
    level_offsets.append(offset)

  return level_offsets, tree_size


# See system/extras/libfec/include/fec/io.h for these definitions.
FEC_FOOTER_FORMAT = '<LLLLLQ32s'
FEC_MAGIC = 0xfecfecfe


def calc_fec_data_size(image_size, num_roots):
  """Calculates how much space FEC data will take.

  Arguments:
    image_size: The size of the image.
    num_roots: Number of roots.

  Returns:
    The number of bytes needed for FEC for an image of the given size
    and with the requested number of FEC roots.

  Raises:
    ValueError: If output from the 'fec' tool is invalid.
  """
  p = subprocess.Popen(
      ['fec', '--print-fec-size', str(image_size), '--roots', str(num_roots)],
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)
  (pout, perr) = p.communicate()
  retcode = p.wait()
  if retcode != 0:
    raise ValueError('Error invoking fec: {}'.format(perr))
  return int(pout)


def generate_fec_data(image_filename, num_roots):
  """Generate FEC codes for an image.

  Arguments:
    image_filename: The filename of the image.
    num_roots: Number of roots.

  Returns:
    The FEC data blob as bytes.

  Raises:
    ValueError: If calling the 'fec' tool failed or the output is invalid.
  """
  with tempfile.NamedTemporaryFile() as fec_tmpfile:
    try:
      subprocess.check_call(
          ['fec', '--encode', '--roots', str(num_roots), image_filename,
           fec_tmpfile.name],
          stderr=open(os.devnull, 'wb'))
    except subprocess.CalledProcessError as e:
      raise ValueError('Execution of \'fec\' tool failed: {}.'
                       .format(e)) from e
    fec_data = fec_tmpfile.read()

  footer_size = struct.calcsize(FEC_FOOTER_FORMAT)
  footer_data = fec_data[-footer_size:]
  (magic, _, _, num_roots, fec_size, _, _) = struct.unpack(FEC_FOOTER_FORMAT,
                                                           footer_data)
  if magic != FEC_MAGIC:
    raise ValueError('Unexpected magic in FEC footer')
  return fec_data[0:fec_size]


def generate_hash_tree(image, image_size, block_size, hash_alg_name, salt,
                       digest_padding, hash_level_offsets, tree_size):
  """Generates a Merkle-tree for a file.

  Arguments:
    image: The image, as a file.
    image_size: The size of the image.
    block_size: The block size, e.g. 4096.
    hash_alg_name: The hash algorithm, e.g. 'sha256' or 'sha1'.
    salt: The salt to use.
    digest_padding: The padding for each digest.
    hash_level_offsets: The offsets from calc_hash_level_offsets().
    tree_size: The size of the tree, in number of bytes.

  Returns:
    A tuple where the first element is the top-level hash as bytes and the
    second element is the hash-tree as bytes.
  """
  hash_ret = bytearray(tree_size)
  hash_src_offset = 0
  hash_src_size = image_size
  level_num = 0

  # If there is only one block, returns the top-level hash directly.
  if hash_src_size == block_size:
    hasher = create_avb_hashtree_hasher(hash_alg_name, salt)
    image.seek(0)
    hasher.update(image.read(block_size))
    return hasher.digest(), bytes(hash_ret)

  while hash_src_size > block_size:
    level_output_list = []
    remaining = hash_src_size
    while remaining > 0:
      hasher = create_avb_hashtree_hasher(hash_alg_name, salt)
      # Only read from the file for the first level - for subsequent
      # levels, access the array we're building.
      if level_num == 0:
        image.seek(hash_src_offset + hash_src_size - remaining)
        data = image.read(min(remaining, block_size))
      else:
        offset = hash_level_offsets[level_num - 1] + hash_src_size - remaining
        data = hash_ret[offset:offset + block_size]
      hasher.update(data)

      remaining -= len(data)
      if len(data) < block_size:
        hasher.update(b'\0' * (block_size - len(data)))
      level_output_list.append(hasher.digest())
      if digest_padding > 0:
        level_output_list.append(b'\0' * digest_padding)

    level_output = b''.join(level_output_list)

    padding_needed = (round_to_multiple(
        len(level_output), block_size) - len(level_output))
    level_output += b'\0' * padding_needed

    # Copy level-output into resulting tree.
    offset = hash_level_offsets[level_num]
    hash_ret[offset:offset + len(level_output)] = level_output

    # Continue on to the next level.
    hash_src_size = len(level_output)
    level_num += 1

  hasher = create_avb_hashtree_hasher(hash_alg_name, salt)
  hasher.update(level_output)
  return hasher.digest(), bytes(hash_ret)


class AvbTool(object):
  """Object for avbtool command-line tool."""

  def __init__(self):
    """Initializer method."""
    self.avb = Avb()

  def _add_common_args(self, sub_parser):
    """Adds arguments used by several sub-commands.

    Arguments:
      sub_parser: The parser to add arguments to.
    """
    sub_parser.add_argument('--algorithm',
                            help='Algorithm to use (default: NONE)',
                            metavar='ALGORITHM',
                            default='NONE')
    sub_parser.add_argument('--key',
                            help='Path to RSA private key file',
                            metavar='KEY',
                            required=False)
    sub_parser.add_argument('--signing_helper',
                            help='Path to helper used for signing',
                            metavar='APP',
                            default=None,
                            required=False)
    sub_parser.add_argument('--signing_helper_with_files',
                            help='Path to helper used for signing using files',
                            metavar='APP',
                            default=None,
                            required=False)
    sub_parser.add_argument('--public_key_metadata',
                            help='Path to public key metadata file',
                            metavar='KEY_METADATA',
                            required=False)
    sub_parser.add_argument('--rollback_index',
                            help='Rollback Index',
                            type=parse_number,
                            default=0)
    sub_parser.add_argument('--rollback_index_location',
                            help='Location of main vbmeta Rollback Index',
                            type=parse_number,
                            default=0)
    # This is used internally for unit tests. Do not include in --help output.
    sub_parser.add_argument('--internal_release_string',
                            help=argparse.SUPPRESS)
    sub_parser.add_argument('--append_to_release_string',
                            help='Text to append to release string',
                            metavar='STR')
    sub_parser.add_argument('--prop',
                            help='Add property',
                            metavar='KEY:VALUE',
                            action='append')
    sub_parser.add_argument('--prop_from_file',
                            help='Add property from file',
                            metavar='KEY:PATH',
                            action='append')
    sub_parser.add_argument('--kernel_cmdline',
                            help='Add kernel cmdline',
                            metavar='CMDLINE',
                            action='append')
    # TODO(zeuthen): the --setup_rootfs_from_kernel option used to be called
    # --generate_dm_verity_cmdline_from_hashtree. Remove support for the latter
    # at some future point.
    sub_parser.add_argument('--setup_rootfs_from_kernel',
                            '--generate_dm_verity_cmdline_from_hashtree',
                            metavar='IMAGE',
                            help='Adds kernel cmdline to set up IMAGE',
                            type=argparse.FileType('rb'))
    sub_parser.add_argument('--include_descriptors_from_image',
                            help='Include descriptors from image',
                            metavar='IMAGE',
                            action='append',
                            type=argparse.FileType('rb'))
    sub_parser.add_argument('--print_required_libavb_version',
                            help=('Don\'t store the footer - '
                                  'instead calculate the required libavb '
                                  'version for the given options.'),
                            action='store_true')
    # These are only allowed from top-level vbmeta and boot-in-lieu-of-vbmeta.
    sub_parser.add_argument('--chain_partition',
                            help='Allow signed integrity-data for partition',
                            metavar='PART_NAME:ROLLBACK_SLOT:KEY_PATH',
                            action='append')
    sub_parser.add_argument('--chain_partition_do_not_use_ab',
                            help='Allow signed integrity-data for partition does not use A/B',
                            metavar='PART_NAME:ROLLBACK_SLOT:KEY_PATH',
                            action='append',
                            required=False)
    sub_parser.add_argument('--flags',
                            help='VBMeta flags',
                            type=parse_number,
                            default=0)
    sub_parser.add_argument('--set_hashtree_disabled_flag',
                            help='Set the HASHTREE_DISABLED flag',
                            action='store_true')
    sub_parser.add_argument('--set_verification_disabled_flag',
                            help='Set the VERIFICATION_DISABLED flag',
                            action='store_true')

  def _add_common_footer_args(self, sub_parser):
    """Adds arguments used by add_*_footer sub-commands.

    Arguments:
      sub_parser: The parser to add arguments to.
    """
    sub_parser.add_argument('--use_persistent_digest',
                            help='Use a persistent digest on device instead of '
                                 'storing the digest in the descriptor. This '
                                 'cannot be used with A/B so must be combined '
                                 'with --do_not_use_ab when an A/B suffix is '
                                 'expected at runtime.',
                            action='store_true')
    sub_parser.add_argument('--do_not_use_ab',
                            help='The partition does not use A/B even when an '
                                 'A/B suffix is present. This must not be used '
                                 'for vbmeta or chained partitions.',
                            action='store_true')

  def _fixup_common_args(self, args):
    """Common fixups needed by subcommands.

    Arguments:
      args: Arguments to modify.

    Returns:
      The modified arguments.
    """
    if args.set_hashtree_disabled_flag:
      args.flags |= AVB_VBMETA_IMAGE_FLAGS_HASHTREE_DISABLED
    if args.set_verification_disabled_flag:
      args.flags |= AVB_VBMETA_IMAGE_FLAGS_VERIFICATION_DISABLED
    return args

  def run(self, argv):
    """Command-line processor.

    Arguments:
      argv: Pass sys.argv from main.
    """
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(title='subcommands')

    sub_parser = subparsers.add_parser(
        'generate_test_image',
        help=('Generates a test image with a known pattern for testing: '
              '0x00 0x01 0x02 ... 0xff 0x00 0x01 ...'))
    sub_parser.add_argument('--image_size',
                            help='Size of image to generate.',
                            type=parse_number,
                            required=True)
    sub_parser.add_argument('--start_byte',
                            help='Integer for the start byte of the pattern.',
                            type=parse_number,
                            default=0)
    sub_parser.add_argument('--output',
                            help='Output file name.',
                            type=argparse.FileType('wb'),
                            default=sys.stdout)
    sub_parser.set_defaults(func=self.generate_test_image)

    sub_parser = subparsers.add_parser('version',
                                       help='Prints version of avbtool.')
    sub_parser.set_defaults(func=self.version)

    sub_parser = subparsers.add_parser('extract_public_key',
                                       help='Extract public key.')
    sub_parser.add_argument('--key',
                            help='Path to RSA private key file',
                            required=True)
    sub_parser.add_argument('--output',
                            help='Output file name',
                            type=argparse.FileType('wb'),
                            required=True)
    sub_parser.set_defaults(func=self.extract_public_key)

    sub_parser = subparsers.add_parser('extract_public_key_digest',
                                       help='Extract SHA-256 digest of public key.')
    sub_parser.add_argument('--key',
                            help='Path to RSA private key file',
                            required=True)
    sub_parser.add_argument('--output',
                            help='Output file name',
                            type=argparse.FileType('w', encoding='UTF-8'),
                            required=True)
    sub_parser.set_defaults(func=self.extract_public_key_digest)

    sub_parser = subparsers.add_parser('make_vbmeta_image',
                                       help='Makes a vbmeta image.')
    sub_parser.add_argument('--output',
                            help='Output file name',
                            type=argparse.FileType('wb'))
    sub_parser.add_argument('--padding_size',
                            metavar='NUMBER',
                            help='If non-zero, pads output with NUL bytes so '
                                 'its size is a multiple of NUMBER '
                                 '(default: 0)',
                            type=parse_number,
                            default=0)
    self._add_common_args(sub_parser)
    sub_parser.set_defaults(func=self.make_vbmeta_image)

    sub_parser = subparsers.add_parser('add_hash_footer',
                                       help='Add hashes and footer to image.')
    sub_parser.add_argument('--image',
                            help='Image to add hashes to')
    sub_parser.add_argument('--partition_size',
                            help='Partition size',
                            type=parse_number)
    sub_parser.add_argument('--dynamic_partition_size',
                            help='Calculate partition size based on image size',
                            action='store_true')
    sub_parser.add_argument('--partition_name',
                            help='Partition name',
                            default=None)
    sub_parser.add_argument('--hash_algorithm',
                            help='Hash algorithm to use (default: sha256)',
                            default='sha256')
    sub_parser.add_argument('--salt',
                            help='Salt in hex (default: /dev/urandom)')
    sub_parser.add_argument('--calc_max_image_size',
                            help=('Don\'t store the footer - '
                                  'instead calculate the maximum image size '
                                  'leaving enough room for metadata with '
                                  'the given partition size.'),
                            action='store_true')
    sub_parser.add_argument('--output_vbmeta_image',
                            help='Also write vbmeta struct to file',
                            type=argparse.FileType('wb'))
    sub_parser.add_argument('--do_not_append_vbmeta_image',
                            help=('Do not append vbmeta struct or footer '
                                  'to the image'),
                            action='store_true')
    self._add_common_args(sub_parser)
    self._add_common_footer_args(sub_parser)
    sub_parser.set_defaults(func=self.add_hash_footer)

    sub_parser = subparsers.add_parser('append_vbmeta_image',
                                       help='Append vbmeta image to image.')
    sub_parser.add_argument('--image',
                            help='Image to append vbmeta blob to',
                            type=argparse.FileType('rb+'))
    sub_parser.add_argument('--partition_size',
                            help='Partition size',
                            type=parse_number,
                            required=True)
    sub_parser.add_argument('--vbmeta_image',
                            help='Image with vbmeta blob to append',
                            type=argparse.FileType('rb'))
    sub_parser.set_defaults(func=self.append_vbmeta_image)

    sub_parser = subparsers.add_parser(
        'add_hashtree_footer',
        help='Add hashtree and footer to image.')
    sub_parser.add_argument('--image',
                            help='Image to add hashtree to',
                            type=argparse.FileType('rb+'))
    sub_parser.add_argument('--partition_size',
                            help='Partition size',
                            default=0,
                            type=parse_number)
    sub_parser.add_argument('--partition_name',
                            help='Partition name',
                            default='')
    # For backwards compatibility, add_hashtree_footer defaults to sha1, even
    # though sha1 is not actually allowed to be used in Android. At the earliest
    # opportunity, the default should be fixed to be sha256. For now we just
    # print a warning when the algorithm defaults to sha1. Below uses default=''
    # so that defaulted sha1 can be distinguished from explicit sha1.
    sub_parser.add_argument('--hash_algorithm',
                            help='Hash algorithm to use (default: sha1)',
                            default='')
    sub_parser.add_argument('--salt',
                            help='Salt in hex (default: /dev/urandom)')
    sub_parser.add_argument('--block_size',
                            help='Block size (default: 4096)',
                            type=parse_number,
                            default=4096)
    # TODO(zeuthen): The --generate_fec option was removed when we
    # moved to generating FEC by default. To avoid breaking existing
    # users needing to transition we simply just print a warning below
    # in add_hashtree_footer(). Remove this option and the warning at
    # some point in the future.
    sub_parser.add_argument('--generate_fec',
                            help=argparse.SUPPRESS,
                            action='store_true')
    sub_parser.add_argument(
        '--do_not_generate_fec',
        help='Do not generate forward-error-correction codes',
        action='store_true')
    sub_parser.add_argument('--fec_num_roots',
                            help='Number of roots for FEC (default: 2)',
                            type=parse_number,
                            default=2)
    sub_parser.add_argument('--calc_max_image_size',
                            help=('Don\'t store the hashtree or footer - '
                                  'instead calculate the maximum image size '
                                  'leaving enough room for hashtree '
                                  'and metadata with the given partition '
                                  'size.'),
                            action='store_true')
    sub_parser.add_argument('--output_vbmeta_image',
                            help='Also write vbmeta struct to file',
                            type=argparse.FileType('wb'))
    sub_parser.add_argument('--do_not_append_vbmeta_image',
                            help=('Do not append vbmeta struct or footer '
                                  'to the image'),
                            action='store_true')
    # This is different from --setup_rootfs_from_kernel insofar that
    # it doesn't take an IMAGE, the generated cmdline will be for the
    # hashtree we're adding.
    sub_parser.add_argument('--setup_as_rootfs_from_kernel',
                            action='store_true',
                            help='Adds kernel cmdline for setting up rootfs')
    sub_parser.add_argument('--no_hashtree',
                            action='store_true',
                            help='Do not append hashtree')
    sub_parser.add_argument('--check_at_most_once',
                            action='store_true',
                            help='Set to verify data block only once')
    self._add_common_args(sub_parser)
    self._add_common_footer_args(sub_parser)
    sub_parser.set_defaults(func=self.add_hashtree_footer)

    sub_parser = subparsers.add_parser('erase_footer',
                                       help='Erase footer from an image.')
    sub_parser.add_argument('--image',
                            help='Image with a footer',
                            type=argparse.FileType('rb+'),
                            required=True)
    sub_parser.add_argument('--keep_hashtree',
                            help='Keep the hashtree and FEC in the image',
                            action='store_true')
    sub_parser.set_defaults(func=self.erase_footer)

    sub_parser = subparsers.add_parser('zero_hashtree',
                                       help='Zero out hashtree and FEC data.')
    sub_parser.add_argument('--image',
                            help='Image with a footer',
                            type=argparse.FileType('rb+'),
                            required=True)
    sub_parser.set_defaults(func=self.zero_hashtree)

    sub_parser = subparsers.add_parser(
        'extract_vbmeta_image',
        help='Extracts vbmeta from an image with a footer.')
    sub_parser.add_argument('--image',
                            help='Image with footer',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--output',
                            help='Output file name',
                            type=argparse.FileType('wb'))
    sub_parser.add_argument('--padding_size',
                            metavar='NUMBER',
                            help='If non-zero, pads output with NUL bytes so '
                                 'its size is a multiple of NUMBER '
                                 '(default: 0)',
                            type=parse_number,
                            default=0)
    sub_parser.set_defaults(func=self.extract_vbmeta_image)

    sub_parser = subparsers.add_parser('resize_image',
                                       help='Resize image with a footer.')
    sub_parser.add_argument('--image',
                            help='Image with a footer',
                            type=argparse.FileType('rb+'),
                            required=True)
    sub_parser.add_argument('--partition_size',
                            help='New partition size',
                            type=parse_number)
    sub_parser.set_defaults(func=self.resize_image)

    sub_parser = subparsers.add_parser(
        'info_image',
        help='Show information about vbmeta or footer.')
    sub_parser.add_argument('--image',
                            help='Image to show information about',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--output',
                            help='Write info to file',
                            type=argparse.FileType('wt'),
                            default=sys.stdout)
    sub_parser.add_argument('--cert', '--atx',
                            help=('Show information about the avb_cert '
                                  'extension certificate.'),
                            action='store_true')
    sub_parser.add_argument('--output_pubkey',
                            help='Write public key to file',
                            type=argparse.FileType('wb'),
                            required=False)
    sub_parser.set_defaults(func=self.info_image)

    sub_parser = subparsers.add_parser(
        'verify_image',
        help='Verify an image.')
    sub_parser.add_argument('--image',
                            help='Image to verify',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--key',
                            help='Check embedded public key matches KEY',
                            metavar='KEY',
                            required=False)
    sub_parser.add_argument('--expected_chain_partition',
                            help='Expected chain partition',
                            metavar='PART_NAME:ROLLBACK_SLOT:KEY_PATH',
                            action='append')
    sub_parser.add_argument(
        '--follow_chain_partitions',
        help=('Follows chain partitions even when not '
              'specified with the --expected_chain_partition option'),
        action='store_true')
    sub_parser.add_argument(
        '--accept_zeroed_hashtree',
        help=('Accept images where the hashtree or FEC data is zeroed out'),
        action='store_true')
    sub_parser.set_defaults(func=self.verify_image)

    sub_parser = subparsers.add_parser(
        'print_partition_digests',
        help='Prints partition digests.')
    sub_parser.add_argument('--image',
                            help='Image to print partition digests from',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--output',
                            help='Write info to file',
                            type=argparse.FileType('wt'),
                            default=sys.stdout)
    sub_parser.add_argument('--json',
                            help=('Print output as JSON'),
                            action='store_true')
    sub_parser.set_defaults(func=self.print_partition_digests)

    sub_parser = subparsers.add_parser(
        'calculate_vbmeta_digest',
        help='Calculate vbmeta digest.')
    sub_parser.add_argument('--image',
                            help='Image to calculate digest for',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--hash_algorithm',
                            help='Hash algorithm to use (default: sha256)',
                            default='sha256')
    sub_parser.add_argument('--output',
                            help='Write hex digest to file (default: stdout)',
                            type=argparse.FileType('wt'),
                            default=sys.stdout)
    sub_parser.set_defaults(func=self.calculate_vbmeta_digest)

    sub_parser = subparsers.add_parser(
        'calculate_kernel_cmdline',
        help='Calculate kernel cmdline.')
    sub_parser.add_argument('--image',
                            help='Image to calculate kernel cmdline for',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--hashtree_disabled',
                            help='Return the cmdline for hashtree disabled',
                            action='store_true')
    sub_parser.add_argument('--output',
                            help='Write cmdline to file (default: stdout)',
                            type=argparse.FileType('wt'),
                            default=sys.stdout)
    sub_parser.set_defaults(func=self.calculate_kernel_cmdline)

    sub_parser = subparsers.add_parser('set_ab_metadata',
                                       help='Set A/B metadata.')
    sub_parser.add_argument('--misc_image',
                            help=('The misc image to modify. If the image does '
                                  'not exist, it will be created.'),
                            type=argparse.FileType('r+b'),
                            required=True)
    sub_parser.add_argument('--slot_data',
                            help=('Slot data of the form "priority", '
                                  '"tries_remaining", "sucessful_boot" for '
                                  'slot A followed by the same for slot B, '
                                  'separated by colons. The default value '
                                  'is 15:7:0:14:7:0.'),
                            default='15:7:0:14:7:0')
    sub_parser.set_defaults(func=self.set_ab_metadata)

    sub_parser = subparsers.add_parser(
        'make_certificate',
        aliases=['make_atx_certificate'],
        help='Create an avb_cert extension certificate.')
    sub_parser.add_argument('--output',
                            help='Write certificate to file',
                            type=argparse.FileType('wb'),
                            default=sys.stdout)
    sub_parser.add_argument('--subject',
                            help=('Path to subject file'),
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--subject_key',
                            help=('Path to subject RSA public key file'),
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--subject_key_version',
                            help=('Version of the subject key'),
                            type=parse_number,
                            required=False)
    # We have 3 different usage modifying args for convenience, at most one of
    # which can be provided since they all set the same usage field.
    usage_group = sub_parser.add_mutually_exclusive_group(required=False)
    usage_group.add_argument('--subject_is_intermediate_authority',
                             help=('Override usage with the value used for '
                                   'an intermediate authority'),
                             action='store_const',
                             const=CERT_USAGE_INTERMEDIATE_AUTHORITY,
                             required=False)
    usage_group.add_argument('--usage',
                             help=('Override usage with a hash of the provided '
                                   'string'),
                             required=False),
    usage_group.add_argument('--usage_for_unlock',
                             help=('Override usage with the value used for '
                                   'authenticated unlock'),
                             action='store_const',
                             const=CERT_USAGE_UNLOCK,
                             required=False),
    sub_parser.add_argument('--authority_key',
                            help='Path to authority RSA private key file',
                            required=False)
    sub_parser.add_argument('--signing_helper',
                            help='Path to helper used for signing',
                            metavar='APP',
                            default=None,
                            required=False)
    sub_parser.add_argument('--signing_helper_with_files',
                            help='Path to helper used for signing using files',
                            metavar='APP',
                            default=None,
                            required=False)
    sub_parser.set_defaults(func=self.make_certificate)

    sub_parser = subparsers.add_parser(
        'make_cert_permanent_attributes',
        aliases=['make_atx_permanent_attributes'],
        help='Create avb_cert extension permanent attributes.')
    sub_parser.add_argument('--output',
                            help='Write attributes to file',
                            type=argparse.FileType('wb'),
                            default=sys.stdout)
    sub_parser.add_argument('--root_authority_key',
                            help='Path to authority RSA public key file',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--product_id',
                            help=('Path to Product ID file'),
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.set_defaults(func=self.make_cert_permanent_attributes)

    sub_parser = subparsers.add_parser(
        'make_cert_metadata',
        aliases=['make_atx_metadata'],
        help='Create avb_cert extension metadata.')
    sub_parser.add_argument('--output',
                            help='Write metadata to file',
                            type=argparse.FileType('wb'),
                            default=sys.stdout)
    sub_parser.add_argument('--intermediate_key_certificate',
                            help='Path to intermediate key certificate file',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--product_key_certificate',
                            help='Path to product key certificate file',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.set_defaults(func=self.make_cert_metadata)

    sub_parser = subparsers.add_parser(
        'make_cert_unlock_credential',
        aliases=['make_atx_unlock_credential'],
        help='Create an avb_cert extension unlock credential.')
    sub_parser.add_argument('--output',
                            help='Write credential to file',
                            type=argparse.FileType('wb'),
                            default=sys.stdout)
    sub_parser.add_argument('--intermediate_key_certificate',
                            help='Path to intermediate key certificate file',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--unlock_key_certificate',
                            help='Path to unlock key certificate file',
                            type=argparse.FileType('rb'),
                            required=True)
    sub_parser.add_argument('--challenge',
                            help='Path to the challenge to sign (optional). If '
                                 'this is not provided the challenge signature '
                                 'field is omitted and can be concatenated '
                                 'later.',
                            required=False)
    sub_parser.add_argument('--unlock_key',
                            help='Path to unlock key (optional). Must be '
                                 'provided if using --challenge.',
                            required=False)
    sub_parser.add_argument('--signing_helper',
                            help='Path to helper used for signing',
                            metavar='APP',
                            default=None,
                            required=False)
    sub_parser.add_argument('--signing_helper_with_files',
                            help='Path to helper used for signing using files',
                            metavar='APP',
                            default=None,
                            required=False)
    sub_parser.set_defaults(func=self.make_cert_unlock_credential)

    args = parser.parse_args(argv[1:])
    try:
      args.func(args)
    except AttributeError:
      # This error gets raised when the command line tool is called without any
      # arguments. It mimics the original Python 2 behavior.
      parser.print_usage()
      print('avbtool: error: too few arguments')
      sys.exit(2)
    except AvbError as e:
      sys.stderr.write('{}: {}\n'.format(argv[0], str(e)))
      sys.exit(1)

  def version(self, _):
    """Implements the 'version' sub-command."""
    print(get_release_string())

  def generate_test_image(self, args):
    """Implements the 'generate_test_image' sub-command."""
    self.avb.generate_test_image(args.output, args.image_size, args.start_byte)

  def extract_public_key(self, args):
    """Implements the 'extract_public_key' sub-command."""
    self.avb.extract_public_key(args.key, args.output)

  def extract_public_key_digest(self, args):
    """Implements the 'extract_public_key_digest' sub-command."""
    self.avb.extract_public_key_digest(args.key, args.output)

  def make_vbmeta_image(self, args):
    """Implements the 'make_vbmeta_image' sub-command."""
    args = self._fixup_common_args(args)
    self.avb.make_vbmeta_image(args.output, args.chain_partition,
                               args.chain_partition_do_not_use_ab,
                               args.algorithm, args.key,
                               args.public_key_metadata, args.rollback_index,
                               args.flags, args.rollback_index_location,
                               args.prop, args.prop_from_file,
                               args.kernel_cmdline,
                               args.setup_rootfs_from_kernel,
                               args.include_descriptors_from_image,
                               args.signing_helper,
                               args.signing_helper_with_files,
                               args.internal_release_string,
                               args.append_to_release_string,
                               args.print_required_libavb_version,
                               args.padding_size)

  def append_vbmeta_image(self, args):
    """Implements the 'append_vbmeta_image' sub-command."""
    self.avb.append_vbmeta_image(args.image.name, args.vbmeta_image.name,
                                 args.partition_size)

  def add_hash_footer(self, args):
    """Implements the 'add_hash_footer' sub-command."""
    args = self._fixup_common_args(args)
    self.avb.add_hash_footer(args.image,
                             args.partition_size, args.dynamic_partition_size,
                             args.partition_name, args.hash_algorithm,
                             args.salt, args.chain_partition,
                             args.chain_partition_do_not_use_ab,
                             args.algorithm, args.key,
                             args.public_key_metadata, args.rollback_index,
                             args.flags, args.rollback_index_location,
                             args.prop, args.prop_from_file,
                             args.kernel_cmdline,
                             args.setup_rootfs_from_kernel,
                             args.include_descriptors_from_image,
                             args.calc_max_image_size,
                             args.signing_helper,
                             args.signing_helper_with_files,
                             args.internal_release_string,
                             args.append_to_release_string,
                             args.output_vbmeta_image,
                             args.do_not_append_vbmeta_image,
                             args.print_required_libavb_version,
                             args.use_persistent_digest,
                             args.do_not_use_ab)

  def add_hashtree_footer(self, args):
    """Implements the 'add_hashtree_footer' sub-command."""
    args = self._fixup_common_args(args)
    # TODO(zeuthen): Remove when removing support for the
    # '--generate_fec' option above.
    if args.generate_fec:
      sys.stderr.write('The --generate_fec option is deprecated since FEC '
                       'is now generated by default. Use the option '
                       '--do_not_generate_fec to not generate FEC.\n')
    if args.hash_algorithm == '':
      args.hash_algorithm = 'sha1'
      # In --calc_max_image_size mode don't show the sha1 warning, since sha1
      # digests get padded to the same size as sha256 anyway.
      if not args.calc_max_image_size:
        sys.stderr.write(
"""Warning: 'avbtool add_hashtree_footer' executed without an explicit
--hash_algorithm option. Defaulting to sha1 for backwards compatibility.
Please use '--hash_algorithm sha256'.
""")
    self.avb.add_hashtree_footer(
        args.image.name if args.image else None,
        args.partition_size,
        args.partition_name,
        not args.do_not_generate_fec, args.fec_num_roots,
        args.hash_algorithm, args.block_size,
        args.salt, args.chain_partition,
        args.chain_partition_do_not_use_ab,
        args.algorithm,
        args.key, args.public_key_metadata,
        args.rollback_index, args.flags,
        args.rollback_index_location, args.prop,
        args.prop_from_file,
        args.kernel_cmdline,
        args.setup_rootfs_from_kernel,
        args.setup_as_rootfs_from_kernel,
        args.include_descriptors_from_image,
        args.calc_max_image_size,
        args.signing_helper,
        args.signing_helper_with_files,
        args.internal_release_string,
        args.append_to_release_string,
        args.output_vbmeta_image,
        args.do_not_append_vbmeta_image,
        args.print_required_libavb_version,
        args.use_persistent_digest,
        args.do_not_use_ab,
        args.no_hashtree,
        args.check_at_most_once)

  def erase_footer(self, args):
    """Implements the 'erase_footer' sub-command."""
    self.avb.erase_footer(args.image.name, args.keep_hashtree)

  def zero_hashtree(self, args):
    """Implements the 'zero_hashtree' sub-command."""
    self.avb.zero_hashtree(args.image.name)

  def extract_vbmeta_image(self, args):
    """Implements the 'extract_vbmeta_image' sub-command."""
    self.avb.extract_vbmeta_image(args.output, args.image.name,
                                  args.padding_size)

  def resize_image(self, args):
    """Implements the 'resize_image' sub-command."""
    self.avb.resize_image(args.image.name, args.partition_size)

  def set_ab_metadata(self, args):
    """Implements the 'set_ab_metadata' sub-command."""
    self.avb.set_ab_metadata(args.misc_image, args.slot_data)

  def info_image(self, args):
    """Implements the 'info_image' sub-command."""
    self.avb.info_image(args.image.name, args.output,
                        args.cert, args.output_pubkey)

  def verify_image(self, args):
    """Implements the 'verify_image' sub-command."""
    self.avb.verify_image(args.image.name, args.key,
                          args.expected_chain_partition,
                          args.follow_chain_partitions,
                          args.accept_zeroed_hashtree)

  def print_partition_digests(self, args):
    """Implements the 'print_partition_digests' sub-command."""
    self.avb.print_partition_digests(args.image.name, args.output, args.json)

  def calculate_vbmeta_digest(self, args):
    """Implements the 'calculate_vbmeta_digest' sub-command."""
    self.avb.calculate_vbmeta_digest(args.image.name, args.hash_algorithm,
                                     args.output)

  def calculate_kernel_cmdline(self, args):
    """Implements the 'calculate_kernel_cmdline' sub-command."""
    self.avb.calculate_kernel_cmdline(args.image.name, args.hashtree_disabled,
                                      args.output)

  def make_certificate(self, args):
    """Implements the 'make_certificate' sub-command."""
    # argparse mutually exclusive group ensures that at most one of the usage
    # args will exist. If none exist, default to signing usage.
    usage = (args.subject_is_intermediate_authority or args.usage or
             args.usage_for_unlock or CERT_USAGE_SIGNING)
    self.avb.make_certificate(args.output, args.authority_key,
                              args.subject_key.name,
                              args.subject_key_version,
                              args.subject.read(),
                              usage,
                              args.signing_helper,
                              args.signing_helper_with_files)

  def make_cert_permanent_attributes(self, args):
    """Implements the 'make_cert_permanent_attributes' sub-command."""
    self.avb.make_cert_permanent_attributes(args.output,
                                           args.root_authority_key.name,
                                           args.product_id.read())

  def make_cert_metadata(self, args):
    """Implements the 'make_cert_metadata' sub-command."""
    self.avb.make_cert_metadata(args.output,
                               args.intermediate_key_certificate.read(),
                               args.product_key_certificate.read())

  def make_cert_unlock_credential(self, args):
    """Implements the 'make_cert_unlock_credential' sub-command."""
    self.avb.make_cert_unlock_credential(
        args.output,
        args.intermediate_key_certificate.read(),
        args.unlock_key_certificate.read(),
        args.challenge,
        args.unlock_key,
        args.signing_helper,
        args.signing_helper_with_files)


if __name__ == '__main__':
  if AVB_INVOCATION_LOGFILE:
    with open(AVB_INVOCATION_LOGFILE, 'a') as log:
      log.write(' '.join(sys.argv))
      log.write('\n')

  tool = AvbTool()
  tool.run(sys.argv)
