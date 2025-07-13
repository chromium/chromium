from response import respond
import socket

def handle(conn: socket.socket, payload: str):
    respond(conn, "ok", "I am alive!")
